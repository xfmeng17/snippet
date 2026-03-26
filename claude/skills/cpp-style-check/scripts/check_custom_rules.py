#!/usr/bin/env python3
"""
C++ 自定义规则检查脚本

检查 Google C++ Style Guide 之外的项目特有规则：
1. 禁止 try-catch / throw（ERROR）
2. 限制 auto 使用（WARNING）
3. 强制 if/for/while/else 大括号（WARNING）
4. 禁止会抛异常的数值解析函数（ERROR）

用法:
    python3 check_custom_rules.py <文件或目录路径>
    python3 check_custom_rules.py src/search.cc
    python3 check_custom_rules.py src/ --severity error
"""

import argparse
import os
import re
import sys
from dataclasses import dataclass
from pathlib import Path


@dataclass
class Issue:
    file: str
    line: int
    rule: str
    severity: str  # ERROR / WARNING
    message: str


# ============================================================
# 规则 1: 禁止 try-catch 和异常机制
# ============================================================

_EXCEPTION_PATTERNS = [
    (re.compile(r'\btry\s*\{'), 'try block'),
    (re.compile(r'\bcatch\s*\('), 'catch block'),
    (re.compile(r'\bthrow\b'), 'throw statement'),
    (re.compile(r'\bstd::exception_ptr\b'), 'std::exception_ptr'),
    (re.compile(r'\bstd::current_exception\b'), 'std::current_exception'),
    (re.compile(r'\bstd::rethrow_exception\b'), 'std::rethrow_exception'),
]


def check_no_exceptions(lines, filepath):
    """禁止 try-catch 和异常机制"""
    issues = []
    for i, line in enumerate(lines, 1):
        stripped = line.lstrip()
        # 跳过注释行
        if stripped.startswith('//') or stripped.startswith('/*') or stripped.startswith('*'):
            continue
        for pattern, desc in _EXCEPTION_PATTERNS:
            if pattern.search(line):
                issues.append(Issue(
                    file=filepath, line=i, rule='no-exceptions',
                    severity='ERROR',
                    message=f'禁止使用 C++ 异常机制: 检测到 {desc}。用返回值 (bool/Status) 报告错误',
                ))
                break  # 一行只报一次
    return issues


# ============================================================
# 规则 2: 限制 auto 使用
# ============================================================

# 匹配 auto 变量声明（不在注释中）
_AUTO_DECL = re.compile(r'\bauto\b')
# 允许的 auto 场景
_AUTO_ALLOWED = [
    re.compile(r'\bauto\s+\w+\s*=\s*\['),          # lambda: auto fn = [
    re.compile(r'\bauto\s+it\b'),                    # iterator: auto it
    re.compile(r'\bauto\s+iter\b'),                  # iterator: auto iter
    re.compile(r'\bauto\s+\w+\s*=\s*\w+\.begin\('), # auto x = m.begin()
    re.compile(r'\bauto\s+\w+\s*=\s*\w+\.end\('),   # auto x = m.end()
    re.compile(r'\bauto\s+\w+\s*=\s*\w+\.find\('),  # auto x = m.find()
    re.compile(r'\bauto\s+\w+\s*=\s*std::find'),     # auto x = std::find
    re.compile(r'\bauto\s+\w+\s*=\s*std::lower_bound'), # STL iterator
    re.compile(r'\bauto\s+\w+\s*=\s*std::upper_bound'),
    re.compile(r'\bauto&?\s*\['),                    # structured binding: auto [k, v]
    re.compile(r'\bauto&?\s*\]'),                    # continuation of structured binding
]


def check_auto_usage(lines, filepath):
    """限制 auto 使用：仅允许迭代器和 lambda"""
    issues = []
    in_block_comment = False
    for i, line in enumerate(lines, 1):
        stripped = line.lstrip()

        # 跟踪块注释
        if '/*' in stripped:
            in_block_comment = True
        if '*/' in stripped:
            in_block_comment = False
            continue
        if in_block_comment or stripped.startswith('//') or stripped.startswith('*'):
            continue

        if not _AUTO_DECL.search(line):
            continue

        # 检查是否属于允许的场景
        allowed = False
        for pattern in _AUTO_ALLOWED:
            if pattern.search(line):
                allowed = True
                break

        if not allowed:
            issues.append(Issue(
                file=filepath, line=i, rule='restrict-auto',
                severity='WARNING',
                message=f'避免使用 auto，请写明确的类型名（仅 iterator/lambda 可用 auto）',
            ))
    return issues


# ============================================================
# 规则 3: 强制大括号
# ============================================================

# 匹配 if/for/while/else 后面没有 { 的情况
# 匹配完整的控制流语句（括号匹配完成后没有 {）
# 注意: 多行 if 条件（括号未闭合）不应匹配
_CONTROL_FLOW_ONELINER = re.compile(
    r'^\s*'
    r'(?:if\s*\(.*\)|else\s+if\s*\(.*\)|else|for\s*\(.*\)|while\s*\(.*\))'
    r'\s+(?!\{)\S'
)


def _parens_balanced(line):
    """检查一行中的括号是否平衡（粗略检测，忽略字符串/注释中的括号）"""
    depth = 0
    for ch in line:
        if ch == '(':
            depth += 1
        elif ch == ')':
            depth -= 1
    return depth == 0


def check_braces(lines, filepath):
    """if/for/while/else 必须使用 {}"""
    issues = []
    for i, line in enumerate(lines, 1):
        stripped = line.lstrip()
        if stripped.startswith('//') or stripped.startswith('/*') or stripped.startswith('*'):
            continue

        # 情况 1: 控制语句独占一行，下一行不是 {
        # 需要完整匹配: if/for/while 的括号必须闭合（排除多行条件）
        ctrl_match = re.match(
            r'^\s*(?:if\s*\(|else\s+if\s*\(|else|for\s*\(|while\s*\()', line)
        if ctrl_match:
            # else 单独一行（无括号）
            is_else_only = re.match(r'^\s*else\s*$', line)
            # if/for/while: 检查括号是否闭合
            is_paren_stmt = re.match(
                r'^\s*(?:if|else\s+if|for|while)\s*\(', line)
            has_balanced_parens = _parens_balanced(line) if is_paren_stmt else True

            if (is_else_only or (is_paren_stmt and has_balanced_parens)):
                # 整行只有控制语句（无语句体）
                line_after_ctrl = line.rstrip()
                # 去掉控制语句后看是否只剩 ) 或空
                ends_with_paren = line_after_ctrl.rstrip().endswith(')')
                is_standalone = is_else_only or ends_with_paren

                if is_standalone and i < len(lines):
                    next_line = lines[i].lstrip()
                    if next_line and not next_line.startswith('{') and not next_line.startswith('//'):
                        issues.append(Issue(
                            file=filepath, line=i, rule='require-braces',
                            severity='WARNING',
                            message='if/for/while/else 语句体必须使用 {}，即使只有一行',
                        ))

        # 情况 2: 单行写法 if (cond) statement; — 括号必须闭合
        if _CONTROL_FLOW_ONELINER.match(line) and _parens_balanced(line):
            # 排除 if (cond) { ... } 的情况
            if '{' not in line:
                issues.append(Issue(
                    file=filepath, line=i, rule='require-braces',
                    severity='WARNING',
                    message='if/for/while/else 语句体必须使用 {}，即使只有一行',
                ))
    return issues


# ============================================================
# 规则 4: 禁止会抛异常的数值解析函数
# ============================================================

_THROWING_PARSERS = re.compile(
    r'\bstd::(stoi|stol|stoll|stoul|stoull|stof|stod|stold)\b'
)


def check_no_throwing_parsers(lines, filepath):
    """禁止 std::stoi/stof 等会抛异常的函数"""
    issues = []
    for i, line in enumerate(lines, 1):
        stripped = line.lstrip()
        if stripped.startswith('//') or stripped.startswith('/*') or stripped.startswith('*'):
            continue
        match = _THROWING_PARSERS.search(line)
        if match:
            func = match.group(0)
            issues.append(Issue(
                file=filepath, line=i, rule='no-throwing-parsers',
                severity='ERROR',
                message=f'禁止使用 {func}（会抛异常）。整数用 std::from_chars，浮点用 std::strtof/strtod',
            ))
    return issues


# ============================================================
# 主逻辑
# ============================================================

ALL_CHECKS = [
    check_no_exceptions,
    check_auto_usage,
    check_braces,
    check_no_throwing_parsers,
]

CPP_EXTENSIONS = {'.h', '.cc', '.cpp', '.cxx', '.hpp', '.hxx', '.c'}


def check_file(filepath):
    """检查单个文件"""
    try:
        with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
            lines = f.readlines()
    except OSError as e:
        print(f"无法读取文件 {filepath}: {e}", file=sys.stderr)
        return []

    all_issues = []
    for check_fn in ALL_CHECKS:
        all_issues.extend(check_fn(lines, filepath))
    return all_issues


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
    # 排除 bazel 输出目录和第三方
    files = [f for f in files if '/bazel-' not in f and '/third_party/' not in f
             and '/external/' not in f and '/.cache/' not in f]
    files.sort()
    return files


def main():
    parser = argparse.ArgumentParser(description='C++ 自定义规则检查')
    parser.add_argument('path', help='要检查的文件或目录')
    parser.add_argument('--severity', '-s', choices=['all', 'error', 'warning'],
                        default='all', help='只显示指定级别 (默认: all)')
    args = parser.parse_args()

    files = collect_files(args.path)
    if not files:
        print(f"未找到 C++ 文件: {args.path}")
        sys.exit(0)

    all_issues = []
    for f in files:
        all_issues.extend(check_file(f))

    # 按级别过滤
    if args.severity == 'error':
        all_issues = [i for i in all_issues if i.severity == 'ERROR']
    elif args.severity == 'warning':
        all_issues = [i for i in all_issues if i.severity == 'WARNING']

    if not all_issues:
        print(f"✅ 自定义规则检查通过 ({len(files)} 个文件)")
        sys.exit(0)

    # 统计
    errors = sum(1 for i in all_issues if i.severity == 'ERROR')
    warnings = sum(1 for i in all_issues if i.severity == 'WARNING')

    print(f"❌ 发现 {len(all_issues)} 个问题 ({errors} errors, {warnings} warnings):\n")

    for issue in all_issues:
        icon = '🔴' if issue.severity == 'ERROR' else '🟡'
        print(f"  {icon} {issue.file}:{issue.line} [{issue.rule}] {issue.message}")

    print()
    sys.exit(1)


if __name__ == '__main__':
    main()
