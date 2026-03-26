# cpp-style-check

C++ 代码风格检查 Claude Code Skill，基于 Google C++ Style Guide。

## 四层检查体系

1. **clang-format** — 代码格式化
2. **cpplint** — Google C++ Style Guide 规范
3. **clang-tidy** — 认知复杂度检测
4. **自定义规则** — 禁止异常、限制 auto、强制大括号、禁止抛异常的数值解析

## 安装

```bash
cp -r cpp-style-check ~/.claude/skills/
```

### 依赖

```bash
# Ubuntu/Debian
sudo apt install clang-format clang-tidy
pip install cpplint

# macOS
brew install llvm cpplint
```

## 使用

在 Claude Code 中：

```
/cpp-style-check src/my_module.cc
```

或独立运行脚本：

```bash
# 一键全检查
python3 scripts/check_all.py src/ --complexity-threshold 15

# 仅自定义规则
python3 scripts/check_custom_rules.py src/my_file.cc
```

## 自定义规则说明

| 规则 | 级别 | 说明 |
|------|------|------|
| no-exceptions | ERROR | 禁止 try/catch/throw，用返回值报告错误 |
| restrict-auto | WARNING | 仅允许 auto 用于迭代器和 lambda |
| require-braces | WARNING | if/for/while/else 必须使用 {} |
| no-throwing-parsers | ERROR | 禁止 std::stoi/stof 等，用 std::from_chars 替代 |

## 文件结构

```
cpp-style-check/
├── SKILL.md                         # Skill 定义 + Google Style 速查
└── scripts/
    ├── check_all.py                 # 一键全检查入口
    └── check_custom_rules.py        # 自定义规则检查
```
