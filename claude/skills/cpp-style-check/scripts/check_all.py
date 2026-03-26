#!/usr/bin/env python3
"""
C++ 一键全检查脚本

依次运行：
1. clang-format --dry-run（格式化检查）
2. cpplint（Google 风格规范）
3. clang-tidy（认知复杂度）
4. 自定义规则检查

用法:
    python3 check_all.py <文件或目录路径> [选项]

示例:
    python3 check_all.py src/search.cc
    python3 check_all.py src/ --complexity-threshold 15 --linelength 100
    python3 check_all.py src/ --skip-format --skip-cpplint
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).parent
CPP_EXTENSIONS = {'.h', '.cc', '.cpp', '.cxx', '.hpp', '.hxx', '.c'}


def collect_files(path):
    """递归收集 C++ 文件"""
    p = Path(path)
    if p.is_file():
        if p.suffix in CPP_EXTENSIONS:
            return [str(p)]
        return []

    files = []
    for ext in CPP_EXTENSIONS:
        files.extend(str(f) for f in p.rglob(f'*{ext}'))
    files = [f for f in files if '/bazel-' not in f and '/third_party/' not in f
             and '/external/' not in f and '/.cache/' not in f]
    files.sort()
    return files


def run_clang_format(files):
    """检查 clang-format 格式化"""
    clang_format = shutil.which('clang-format')
    if not clang_format:
        return None, "⚠️  clang-format 未安装，跳过格式化检查"

    violations = []
    for f in files:
        result = subprocess.run(
            [clang_format, '--dry-run', '-Werror', '--style=file', f],
            capture_output=True, text=True,
        )
        if result.returncode != 0:
            # clang-format 的 diff 输出在 stderr
            output = result.stderr.strip()
            if output:
                violations.append(f"  {f}:\n{output}")
            else:
                violations.append(f"  {f}: 格式不合规")

    if not violations:
        return True, f"✅ clang-format: 所有 {len(files)} 个文件格式合规"

    msg = f"❌ clang-format: {len(violations)} 个文件格式不合规:\n"
    msg += '\n'.join(violations[:10])  # 最多显示 10 个
    if len(violations) > 10:
        msg += f"\n  ... 还有 {len(violations) - 10} 个文件"
    msg += "\n\n  修复: clang-format -i --style=file <file>"
    return False, msg


def run_cpplint(files, linelength):
    """运行 cpplint 检查"""
    cpplint = shutil.which('cpplint')
    if not cpplint:
        return None, "⚠️  cpplint 未安装，跳过风格检查 (pip3 install cpplint)"

    cmd = [
        cpplint,
        f'--linelength={linelength}',
        '--filter=-legal/copyright,-build/include_subdir,-build/c++11,-build/c++17,'
        '-build/header_guard,-build/include_order,-whitespace/indent_namespace,-runtime/string',
        '--quiet',
    ] + files

    result = subprocess.run(cmd, capture_output=True, text=True)
    output = result.stderr.strip()  # cpplint 输出到 stderr

    if result.returncode == 0 or not output:
        return True, f"✅ cpplint: 所有 {len(files)} 个文件风格合规"

    lines = output.split('\n')
    # 最后一行是统计信息
    issue_lines = [l for l in lines if l and not l.startswith('Total errors')]
    count = len(issue_lines)

    msg = f"❌ cpplint: 发现 {count} 个风格问题:\n"
    for line in issue_lines[:15]:
        msg += f"  {line}\n"
    if count > 15:
        msg += f"  ... 还有 {count - 15} 个问题\n"
    return False, msg


def _detect_include_paths(path):
    """自动检测 bazel 项目的 external 依赖 include 路径"""
    extra_args = ['-I.']
    # 查找 bazel-<project>/external/ 下的依赖目录
    search_dir = Path(path) if Path(path).is_dir() else Path(path).parent
    for candidate in search_dir.iterdir() if search_dir.exists() else []:
        if candidate.name.startswith('bazel-') and candidate.is_symlink():
            ext_dir = candidate / 'external'
            if ext_dir.is_dir():
                for dep in ext_dir.iterdir():
                    if dep.is_dir():
                        extra_args.append(f'-isystem{dep}')
                break
    # 也往上级目录找（文件可能在子目录中）
    if len(extra_args) == 1:
        for parent in [search_dir] + list(search_dir.parents):
            for candidate in parent.iterdir():
                if candidate.name.startswith('bazel-') and candidate.is_symlink():
                    ext_dir = candidate / 'external'
                    if ext_dir.is_dir():
                        for dep in ext_dir.iterdir():
                            if dep.is_dir():
                                extra_args.append(f'-isystem{dep}')
                        return extra_args
            if parent == parent.parent:
                break
    return extra_args


def run_clang_tidy(files, threshold):
    """运行 clang-tidy 认知复杂度检查"""
    clang_tidy = shutil.which('clang-tidy')
    if not clang_tidy:
        return None, "⚠️  clang-tidy 未安装，跳过复杂度检查"

    config = (
        f'{{"CheckOptions": ['
        f'{{"key": "readability-function-cognitive-complexity.Threshold", "value": "{threshold}"}}'
        f']}}'
    )
    checks = '-*,readability-function-cognitive-complexity'

    # 自动检测 bazel external 依赖路径
    include_args = _detect_include_paths(files[0] if files else '.')

    violations = []
    for f in files:
        # 只检查 .cc/.cpp 文件（头文件可能缺少上下文）
        if Path(f).suffix in {'.h', '.hpp', '.hxx'}:
            continue

        cmd = [clang_tidy, f'-checks={checks}', f'-config={config}', f,
               '--', '-std=c++17'] + include_args
        result = subprocess.run(
            cmd,
            capture_output=True, text=True,
            timeout=60,
        )
        output = result.stdout.strip()
        if 'warning:' in output:
            for line in output.split('\n'):
                if 'cognitive complexity' in line.lower():
                    violations.append(f"  {line.strip()}")

    if not violations:
        cc_files = [f for f in files if Path(f).suffix not in {'.h', '.hpp', '.hxx'}]
        return True, f"✅ clang-tidy: 所有 {len(cc_files)} 个源文件复杂度在阈值 {threshold} 以下"

    msg = f"❌ clang-tidy: {len(violations)} 个函数复杂度超标 (阈值 {threshold}):\n"
    msg += '\n'.join(violations[:10])
    if len(violations) > 10:
        msg += f"\n  ... 还有 {len(violations) - 10} 个函数"
    return False, msg


def run_custom_rules(path):
    """运行自定义规则检查"""
    script = SCRIPT_DIR / 'check_custom_rules.py'
    result = subprocess.run(
        [sys.executable, str(script), path],
        capture_output=True, text=True,
    )
    output = result.stdout.strip()
    return result.returncode == 0, output


def main():
    parser = argparse.ArgumentParser(description='C++ 一键全检查')
    parser.add_argument('path', help='要检查的文件或目录')
    parser.add_argument('--complexity-threshold', '-c', type=int, default=15,
                        help='认知复杂度阈值 (默认: 15)')
    parser.add_argument('--linelength', '-l', type=int, default=100,
                        help='行宽限制 (默认: 100)')
    parser.add_argument('--skip-format', action='store_true', help='跳过 clang-format')
    parser.add_argument('--skip-cpplint', action='store_true', help='跳过 cpplint')
    parser.add_argument('--skip-tidy', action='store_true', help='跳过 clang-tidy')
    parser.add_argument('--skip-custom', action='store_true', help='跳过自定义规则')
    args = parser.parse_args()

    files = collect_files(args.path)
    if not files:
        print(f"未找到 C++ 文件: {args.path}")
        sys.exit(0)

    print(f"📋 检查 {len(files)} 个 C++ 文件...\n")
    print("=" * 60)

    all_passed = True
    checks_run = 0

    # 1. clang-format
    if not args.skip_format:
        passed, msg = run_clang_format(files)
        print(f"\n[1/4] clang-format (格式化)\n{msg}")
        if passed is False:
            all_passed = False
        if passed is not None:
            checks_run += 1

    # 2. cpplint
    if not args.skip_cpplint:
        passed, msg = run_cpplint(files, args.linelength)
        print(f"\n[2/4] cpplint (Google 风格规范)\n{msg}")
        if passed is False:
            all_passed = False
        if passed is not None:
            checks_run += 1

    # 3. clang-tidy
    if not args.skip_tidy:
        passed, msg = run_clang_tidy(files, args.complexity_threshold)
        print(f"\n[3/4] clang-tidy (认知复杂度)\n{msg}")
        if passed is False:
            all_passed = False
        if passed is not None:
            checks_run += 1

    # 4. 自定义规则
    if not args.skip_custom:
        passed, msg = run_custom_rules(args.path)
        print(f"\n[4/4] 自定义规则\n{msg}")
        if not passed:
            all_passed = False
        checks_run += 1

    print("\n" + "=" * 60)
    if all_passed:
        print(f"\n🎉 全部 {checks_run} 项检查通过！")
        sys.exit(0)
    else:
        print(f"\n💡 部分检查未通过，请按上述提示修复。")
        sys.exit(1)


if __name__ == '__main__':
    main()
