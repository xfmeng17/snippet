**English** | [中文](README_CN.md)

# dev-init

> **Note**: This is a reference implementation of a personal development environment setup, containing the author's specific aliases, toolchain paths, and provider configurations.
> Please modify the script contents to suit your own needs before using.

A one-click development environment initialization Claude Code Skill, supporting Docker Linux and macOS.

## Features

- Install base dependencies (zsh, git, vim, cmake, clang-format, cpplint, etc.)
- Install Oh My Zsh + plugins (autosuggestions, syntax-highlighting)
- Generate `~/.zshrc` and `~/.profile` (layered design)
- Generate Claude Code provider configuration (API keys use placeholders)
- Set zsh as the default shell

## Installation

```bash
cp -r dev-init ~/.claude/skills/
```

## Usage

### Via Claude Code

```
/dev-init
```

### Standalone

```bash
bash scripts/init.sh --all       # Install everything
bash scripts/init.sh --deps      # Dependencies only
bash scripts/init.sh --zsh       # Oh My Zsh + .zshrc only
bash scripts/init.sh --profile   # .profile only
bash scripts/init.sh --providers # Provider config only
bash scripts/init.sh --shell     # Switch default shell only
```

## Personalization Notes

The following items reflect the author's personal preferences. After forking, consider modifying:

| File | What to Change |
|------|---------------|
| `gen_zshrc.sh` | Oh My Zsh theme (`fletcherm`), plugin list |
| `gen_profile.sh` | Aliases (`gs/gu/cl/c0/c1/c2/d1/d2`), toolchain paths, conda environment names |
| `gen_providers.sh` | Provider list and BASE_URL |

## File Structure

```
dev-init/
├── SKILL.md
└── scripts/
    ├── init.sh              # One-click initialization entry point
    ├── detect_env.sh        # Environment detection
    ├── install_deps.sh      # Install base dependencies
    ├── install_ohmyzsh.sh   # Oh My Zsh + plugins
    ├── gen_zshrc.sh         # Generate ~/.zshrc
    ├── gen_profile.sh       # Generate ~/.profile
    ├── gen_providers.sh     # Claude Code provider configuration
    └── setup_shell.sh       # Set default shell
```
