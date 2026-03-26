# dev-init

> **注意**: 这是个人开发环境配置的参考实现，包含作者特有的别名、工具链路径和 Provider 配置。
> 直接使用前请根据自己的需求修改脚本内容。

开发环境一键初始化 Claude Code Skill，支持 Docker Linux 和 macOS。

## 功能

- 安装基础依赖 (zsh, git, vim, cmake, clang-format, cpplint 等)
- 安装 Oh My Zsh + 插件 (autosuggestions, syntax-highlighting)
- 生成 `~/.zshrc` 和 `~/.profile`（分层设计）
- 生成 Claude Code Provider 配置（API Key 使用占位符）
- 设置 zsh 为默认 shell

## 安装

```bash
cp -r dev-init ~/.claude/skills/
```

## 使用

### 通过 Claude Code

```
/dev-init
```

### 独立运行

```bash
bash scripts/init.sh --all       # 全部安装
bash scripts/init.sh --deps      # 仅安装依赖
bash scripts/init.sh --zsh       # 仅 Oh My Zsh + .zshrc
bash scripts/init.sh --profile   # 仅 .profile
bash scripts/init.sh --providers # 仅 Provider 配置
bash scripts/init.sh --shell     # 仅切换默认 shell
```

## 个性化要点

以下内容是作者个人偏好，fork 后建议修改：

| 文件 | 需要改的内容 |
|------|-------------|
| `gen_zshrc.sh` | Oh My Zsh 主题 (`fletcherm`)、插件列表 |
| `gen_profile.sh` | 别名 (`gs/gu/cl/c0/c1/c2/d1/d2`)、工具链路径、conda 环境名 |
| `gen_providers.sh` | Provider 列表和 BASE_URL |

## 文件结构

```
dev-init/
├── SKILL.md
└── scripts/
    ├── init.sh              # 一键初始化入口
    ├── detect_env.sh        # 环境检测
    ├── install_deps.sh      # 安装基础依赖
    ├── install_ohmyzsh.sh   # Oh My Zsh + 插件
    ├── gen_zshrc.sh         # 生成 ~/.zshrc
    ├── gen_profile.sh       # 生成 ~/.profile
    ├── gen_providers.sh     # Claude Code Provider 配置
    └── setup_shell.sh       # 设置默认 shell
```
