---
name: dev-init
version: "1.0.0"
description: 在新开发机（Docker Linux 或 macOS）上快速初始化 eggmeng 个人风格的 zsh 开发环境和 Claude Code provider 配置。当用户说"dev-init"、"/dev-init"时触发。支持 "/dev-init export/导出" 和 "/dev-init import/导入"。
allowed-tools:
  - Bash(*)
  - Read
  - Write
  - Edit
  - Glob
  - AskUserQuestion
---

# Dev Init — 开发环境迁移初始化

> **Version**: 1.0.0

在新的开发机上一键初始化个人风格的开发环境。安装脚本位于 `scripts/` 目录。

## 目录结构

~/.claude/skills/dev-init/
├── SKILL.md              # 本文件 (skill 说明)
└── scripts/
    ├── init.sh           # 一键初始化入口 (bash init.sh [--all|--deps|--zsh|--profile|--providers|--shell])
    ├── detect_env.sh     # 环境检测
    ├── install_deps.sh   # 安装基础依赖
    ├── install_ohmyzsh.sh # 安装 Oh My Zsh + 插件
    ├── gen_zshrc.sh      # 生成 ~/.zshrc
    ├── gen_profile.sh    # 生成 ~/.profile
    ├── gen_providers.sh  # 生成 Claude Code Provider 配置
    └── setup_shell.sh    # 设置 zsh 为默认 shell

## 执行流程

### 第 1 步：环境检测

运行 `scripts/detect_env.sh`，自动检测 OS/架构/Docker/已安装工具。
向用户确认要安装哪些组件（可多选）。

### 第 2 步：安装基础依赖

运行 `scripts/install_deps.sh`。

| 项目 | Linux (apt/yum) | macOS (Homebrew) |
|------|-----------------|------------------|
| 包管理器 | apt-get / yum (已有) | Homebrew (若未安装则自动安装，使用 USTC 镜像) |
| 安装列表 | zsh git **vim** curl wget jq build-essential cmake python3 python3-pip **clang-format clang-tidy cpplint** | git curl wget jq cmake python3 |
| vim | **必须安装**（Docker 镜像通常不含 vim） | 系统自带，无需安装 |
| C++ 静态分析 | clang-format, clang-tidy (apt), cpplint (pip) | brew install llvm cpplint |

### 第 3 步：安装 Oh My Zsh

运行 `scripts/install_ohmyzsh.sh`。

通用操作（Linux/macOS 相同）：
- 安装 Oh My Zsh（--unattended 模式）
- 安装插件：zsh-autosuggestions、zsh-syntax-highlighting

### 第 4 步：生成 ~/.zshrc

运行 `scripts/gen_zshrc.sh`。已有文件会自动备份。

通用配置（Linux/macOS 相同）：
- Oh My Zsh + fletcherm 主题
- 插件: git, zsh-autosuggestions, zsh-syntax-highlighting
- source ~/.profile
- PS1 提示符

### 第 5 步：生成 ~/.profile

运行 `scripts/gen_profile.sh`。已有文件会自动备份。

按 **使用维度 × OS 维度** 组织为 3 段（蚂蚁集团配置不包含在 skill 中）：

**1. 通用 (全平台)**:
- 系统基础设置 (ulimit, HISTSIZE, PATH, 颜色)
- 通用别名 (vi, ll, la, gs, gu, p, ..)
- `$HOME/.local/bin` 仅 macOS 加入 PATH（Linux 容器中该目录含 Mac 二进制，不可用）

**2. 开发 — 全平台**:
- C++ gdd 编译函数
- Claude Code (CODEX_HOME, ccuse 函数, claude 包装函数, cl 别名)
- claude() 包装函数自动检测 ccv：有则走 CC-Viewer，无则直接调原生 claude
- LLM API Keys (占位符)

**3. 开发 — macOS only** (`if Darwin`):
- Homebrew (自动检测架构, USTC 镜像)
- Java 1.8 (自动检测 JDK 存在性)
- Maven 3.9.9 (自动检测)
- Python 3.9 + miniforge3 (conda/mamba, conda 初始化, c0/c1/c2 别名)
- Node.js (NVM)
- C++ / LLVM (编译器路径、头文件路径)
- OrbStack (d1 ssh 进入 egg-dev Linux Machine — 通用 C++ Linux 开发环境, d2 docker exec 进入 vsag-dev 容器)

**平台差异**:

| 项目 | Linux | macOS |
|------|-------|-------|
| `~/.local/bin` | 不加入 PATH（含 Mac 二进制） | 加入 PATH |
| ls 颜色 | `dircolors` + `ls --color=auto` | 不生效（Oh My Zsh 已处理） |
| Homebrew | 跳过 | 自动检测架构：arm64→`/opt/homebrew`, x86_64→`/usr/local` |
| Java/Maven/LLVM | 跳过 | 自动检测安装路径，存在则配置 |
| conda 别名 | 跳过 | c0=deactivate, c1=asmemorysrv, c2=agent0 |
| OrbStack alias | 跳过 | d1 ssh egg-dev (通用 C++ Linux 开发环境), d2 docker exec vsag-dev (VSAG 专用容器) |
| ccv 包装 | 有 ccv 走包装，无则直调 claude | 同左 |

### 第 6 步：生成 Claude Code Provider 配置

运行 `scripts/gen_providers.sh`。

在 `~/.claude/` 下生成 4 个 provider 配置文件，API Key 使用占位符：

| Provider | BASE_URL | 默认模型 |
|----------|----------|----------|
| zenmux | `https://zenmux.ai/api/anthropic` | anthropic/claude-opus-4.6 |
| minimax | `https://api.minimaxi.com/anthropic` | MiniMax-M2.7 |

Linux/macOS 无差异，配置文件格式完全相同。

### 第 7 步：设置默认 Shell

运行 `scripts/setup_shell.sh`。

| 场景 | 操作 |
|------|------|
| Docker 容器 | 直接修改 `/etc/passwd`（容器内 chsh 可能不可用） |
| macOS / 普通 Linux | `chsh -s $(which zsh)` |

## 使用方式

### 方式 1: 通过 Claude Code Skill
在 Claude Code 中运行 `/dev-init`，Claude 会按上述流程逐步执行。

### 方式 2: 独立脚本（无需 Claude Code）
将 `scripts/` 目录复制到新机器后直接运行：

# 全部安装
bash scripts/init.sh --all

# 或按需执行单步
bash scripts/init.sh --deps       # 仅安装依赖
bash scripts/init.sh --zsh        # 仅 Oh My Zsh + .zshrc
bash scripts/init.sh --profile    # 仅 .profile
bash scripts/init.sh --providers  # 仅 Provider 配置
bash scripts/init.sh --shell      # 仅切换默认 shell

### 方式 3: 导出/导入（跨机器迁移 skill 本身）

#### 导出 (`/dev-init export` 或 `/dev-init 导出`)

将整个 skill 打包为一个 `.md` 文件，输出到**当前工作目录**，方便拷贝到其他机器。

**导出格式**: `dev-init_export.md`，内容结构如下：

# dev-init skill export
<!-- version: 1.0.0 -->
<!-- exported: 2026-03-20T12:00:00 -->

## FILE: SKILL.md
(SKILL.md 完整内容)

## FILE: scripts/init.sh
(init.sh 完整内容)

## FILE: scripts/detect_env.sh
...

(... 其余 scripts/ 下所有文件 ...)

**执行步骤**:
1. 用 Glob 列出 `~/.claude/skills/dev-init/` 下所有文件（排除空目录）
2. 用 Read 读取每个文件内容，从 SKILL.md frontmatter 提取 `version` 字段
3. 按上述格式拼接（将版本号写入 `<!-- version: x.y.z -->` 头），用 Write 输出到 `{CWD}/dev-init_export.md`
4. 告知用户输出路径和版本号

#### 导入 (`/dev-init import` 或 `/dev-init 导入`)

从当前目录的 `.md` 文件恢复 skill 到 `~/.claude/skills/dev-init/`。

**执行步骤**:
1. 在当前目录查找导入文件。优先找 `dev-init_export.md`；若不存在，列出当前目录的 `.md` 文件让用户选择
2. 用 Read 读取该 `.md` 文件
3. 解析 `<!-- version: x.y.z -->` 获取导入版本，与当前 SKILL.md frontmatter 中的 version 比较
   - 导入版本更新 → 继续导入
   - 导入版本相同 → 提示用户版本相同，确认是否覆盖
   - 导入版本更旧 → 警告用户当前版本更新，确认是否降级
4. 解析每个 `## FILE: <相对路径>` 区块，提取代码块中的文件内容
5. **备份**已有的 `~/.claude/skills/dev-init/` 目录（重命名为 `dev-init.bak.时间戳`）
6. 用 Write 将每个文件写入 `~/.claude/skills/dev-init/<相对路径>`
7. 告知用户恢复结果（包含版本号变化信息）

## 注意事项

- 已存在的配置文件会先备份（`.bak.时间戳`），不会直接覆盖
- API Key 全部使用占位符 `<XXX_API_KEY>`，**绝不写入真实密钥**
- 已移除：蚂蚁集团专有配置、Java/Maven、LLVM、蚂蚁内部 provider
- Homebrew 路径根据 macOS 架构自动适配
- Linux 环境下跳过 Homebrew 相关配置
- 导出文件不含真实密钥（skill 本身就不含）
