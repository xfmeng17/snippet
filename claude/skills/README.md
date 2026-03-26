**English** | [中文](README_CN.md)

# Claude Code Skills

A collection of practical custom Skills for [Claude Code](https://docs.anthropic.com/en/docs/claude-code), covering code learning, C++ style checking, Git committing, and dev environment initialization.

## Skills Overview

| Skill | Description | Generality |
|-------|-------------|------------|
| [learn-project](learn-project/) | Learn open-source projects: source notes / multi-lesson courses / hand-written Mini versions | General |
| [cpp-style-check](cpp-style-check/) | C++ code style checking (Google Style + cognitive complexity + custom rules) | General |
| [gcp](gcp/) | Git commit & push with Google Conventional Commits format | General |
| [dev-init](dev-init/) | One-click dev environment initialization (zsh + Oh My Zsh + Provider config) | Personal reference |

## Installation

Copy the desired skill directories into `~/.claude/skills/`:

```bash
# Install a single skill
cp -r learn-project ~/.claude/skills/

# Install all
cp -r learn-project cpp-style-check gcp dev-init ~/.claude/skills/
```

## What Is a Claude Code Skill

A Skill is a custom instruction extension for Claude Code. Each skill directory contains a `SKILL.md` file that defines:

- **Trigger conditions**: When to activate (slash command or natural language)
- **Workflow**: The specific steps Claude executes
- **Tool permissions**: Which tools are allowed (Bash, Read, Write, etc.)

See the [Claude Code documentation](https://docs.anthropic.com/en/docs/claude-code) for details.

## Skill Descriptions

### learn-project — Deep Learning of Open-Source Projects

Three modes to help you systematically understand a project's How, Why, and Trade-offs:

- **notes**: Organize source code notes by topic
- **course**: Progressive multi-lesson course from scratch (with context decay protection)
- **mini**: Understand the original project by hand-writing a simplified version

```
/learn-project course ~/project/leveldb
```

### cpp-style-check — C++ Code Style Checking

Four-layer checking: clang-format -> cpplint -> clang-tidy (cognitive complexity) -> custom rules.

Custom rules include: no exceptions, restricted `auto` usage, mandatory braces, no exception-throwing numeric parsing functions.

```
/cpp-style-check src/my_module.cc
```

### gcp — Git Commit & Push

Analyzes changes and automatically generates a Conventional Commits formatted commit message, then pushes.

```
/gcp
```

### dev-init — Dev Environment Initialization

> **Note**: This is a personal configuration reference implementation. Please modify the scripts to suit your own needs before use.

One-click zsh dev environment initialization: dependency installation, Oh My Zsh, .zshrc/.profile generation, Claude Code Provider configuration.

```
/dev-init
```

## License

MIT
