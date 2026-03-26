**English** | [中文](README_CN.md)

# cpp-style-check

A C++ code style checking Claude Code Skill based on the Google C++ Style Guide.

## Four-Layer Check System

1. **clang-format** — Code formatting
2. **cpplint** — Google C++ Style Guide compliance
3. **clang-tidy** — Cognitive complexity detection
4. **Custom Rules** — No exceptions, restrict auto, require braces, no throwing parsers

## Installation

```bash
cp -r cpp-style-check ~/.claude/skills/
```

### Dependencies

```bash
# Ubuntu/Debian
sudo apt install clang-format clang-tidy
pip install cpplint

# macOS
brew install llvm cpplint
```

## Usage

In Claude Code:

```
/cpp-style-check src/my_module.cc
```

Or run scripts standalone:

```bash
# Full check
python3 scripts/check_all.py src/ --complexity-threshold 15

# Custom rules only
python3 scripts/check_custom_rules.py src/my_file.cc
```

## Custom Rules

| Rule | Level | Description |
|------|-------|-------------|
| no-exceptions | ERROR | Forbid try/catch/throw; use return values for error reporting |
| restrict-auto | WARNING | Only allow auto for iterators and lambdas |
| require-braces | WARNING | if/for/while/else must use {} |
| no-throwing-parsers | ERROR | Forbid std::stoi/stof etc.; use std::from_chars instead |

## File Structure

```
cpp-style-check/
├── SKILL.md                         # Skill definition + Google Style quick reference
└── scripts/
    ├── check_all.py                 # Full check entry point
    └── check_custom_rules.py        # Custom rules checker
```
