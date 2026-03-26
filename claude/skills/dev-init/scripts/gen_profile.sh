#!/bin/bash
# =============================================================================
# 生成 ~/.profile
# =============================================================================
set -e

TARGET="$HOME/.profile"

# 备份已有文件
if [ -f "$TARGET" ]; then
    BACKUP="${TARGET}.bak.$(date +%s)"
    echo ">>> 备份已有 $TARGET -> $BACKUP"
    cp "$TARGET" "$BACKUP"
fi

echo ">>> 生成 $TARGET"
cat > "$TARGET" << 'PROFILE_EOF'
# =============================================================================
#                         ~/.profile — 通用 Shell 配置
#                    适用于所有 shell (bash, zsh 等)
# =============================================================================
# 结构:
#   1. 通用 (全平台) — 系统基础 + 通用别名
#   2. 开发 — 全平台 — gdd / Claude Code / LLM Keys
#   3. 开发 — macOS only — Homebrew / 工具链 / Conda / Docker
# =============================================================================


# #############################################################################
#                    1. 通用 (全平台)
# #############################################################################

# === 系统基础 ===
ulimit -c unlimited

HISTSIZE=1000
HISTFILESIZE=2000
HISTCONTROL='ignoreboth'

export PATH="$HOME/bin:$PATH"
if [[ "$(uname -s)" == "Darwin" ]]; then
    export PATH="$HOME/.local/bin:$PATH"
fi

# === 颜色 ===
if [ -x /usr/bin/dircolors ]; then
    test -r ~/.dircolors && eval "$(dircolors -b ~/.dircolors)" || eval "$(dircolors -b)"
    alias ls='ls --color=auto'
    alias grep='grep --color=auto'
    alias fgrep='fgrep --color=auto'
    alias egrep='egrep --color=auto'
fi
export LS_COLORS='di=34:ln=35:so=32:pi=33:ex=31:bd=46;31:cd=43;31:su=37;41:sg=30;43:tw=30;42:ow=30;43'
export GCC_COLORS='error=01;31:warning=01;35:note=01;36:caret=01;32:locus=01:quote=01'

# === 通用别名 ===
alias vi='vim'
alias ll='ls -alF'
alias la='ls -A'
alias l='ls'
alias lh='ls -alh'
alias ..='cd ..'

alias gs='git status'
alias gu='git fetch'

alias ip="ifconfig|grep 'inet '"
alias p='python3'


# #############################################################################
#                    2. 开发 — 全平台
# #############################################################################

# === gdd: 快速编译 debug 版 C++ 程序 ===
# 用法: gdd hashmap.cc  -> 生成 debug 版的 hashmap 可执行文件
gdd() {
    if [ -z "$1" ]; then
        echo "用法: gdd <source.cc> [其他g++参数...]"
        return 1
    fi
    local file="$1"
    local output="${file%.*}"
    shift
    echo "编译: g++ -g -O0 -o $output $file $@"
    g++ -g -O0 -o "$output" "$file" "$@"
}

# === Claude Code ===
export CODEX_HOME="$HOME/.claude"

# ccuse: Claude Code Provider 切换
# 用法: ccuse <provider>  切换 provider
#       ccuse             显示当前 provider 和可用列表
ccuse() {
    local claude_dir="$HOME/.claude"
    if [ -z "$1" ]; then
        echo "当前 settings.json:"
        grep -E 'BASE_URL|_MODEL"' "$claude_dir/settings.json" 2>/dev/null | head -4
        echo "\n可用 provider:"
        for f in "$claude_dir"/settings_*.json; do
            local name=$(basename "$f" .json | sed 's/settings_//')
            local url=$(grep 'BASE_URL' "$f" | head -1 | sed 's/.*: *"//;s/".*//')
            printf "  %-20s %s\n" "$name" "$url"
        done
        return 0
    fi
    local target="$claude_dir/settings_${1}.json"
    if [ ! -f "$target" ]; then
        echo "找不到配置: $target"
        ccuse
        return 1
    fi
    cp "$target" "$claude_dir/settings.json"
    echo "已切换到: $1"
    grep -E 'BASE_URL|_MODEL"' "$claude_dir/settings.json" | head -4
}

# claude CLI 包装函数 (CC-Viewer 拦截，部分子命令直通)
claude() {
    if [ "$1" = "--ccv-internal" ]; then
        shift
        command claude "$@"
        return
    fi
    case "$1" in
        doctor|install|update|upgrade|auth|setup-token|agents|plugin|mcp)
            command claude "$@"
            return
            ;;
        --version|-v|--v|--help|-h)
            command claude "$@"
            return
            ;;
    esac
    if command -v ccv >/dev/null 2>&1; then
        ccv run -- claude --ccv-internal "$@"
    else
        command claude "$@"
    fi
}

alias cl='claude'
alias CL='claude'

# === LLM API Keys (占位符，按需替换) ===
# export GOOGLE_CLOUD_PROJECT='<GOOGLE_CLOUD_PROJECT>'
# export ZENMUX_API_KEY='<ZENMUX_API_KEY>'


# #############################################################################
#                    3. 开发 — macOS only
# #############################################################################

if [[ "$(uname -s)" == "Darwin" ]]; then

# === Homebrew ===
if [ "$(uname -m)" = "arm64" ]; then
    export HOMEBREW_HOME=/opt/homebrew
else
    export HOMEBREW_HOME=/usr/local
fi
export PATH="$HOMEBREW_HOME/bin:$PATH"
export HOMEBREW_PIP_INDEX_URL=https://pypi.mirrors.ustc.edu.cn/simple
export HOMEBREW_API_DOMAIN=https://mirrors.ustc.edu.cn/homebrew-bottles/api
export HOMEBREW_BOTTLE_DOMAIN=https://mirrors.ustc.edu.cn/homebrew-bottles
if [ -x "$HOMEBREW_HOME/bin/brew" ]; then
    eval $($HOMEBREW_HOME/bin/brew shellenv)
fi

# === Java (JDK 1.8) ===
if [ -d "/Library/Java/JavaVirtualMachines/jdk-1.8.jdk" ]; then
    export JAVA_HOME="/Library/Java/JavaVirtualMachines/jdk-1.8.jdk/Contents/Home"
    export PATH="$JAVA_HOME/bin:$PATH"
    export CLASSPATH="$JAVA_HOME/lib/tools.jar:$JAVA_HOME/lib/dt.jar:$CLASSPATH"
fi

# === Maven 3.9.9 ===
if [ -d "$HOME/env/apache-maven-3.9.9" ]; then
    export MAVEN_HOME="$HOME/env/apache-maven-3.9.9"
    export PATH="$MAVEN_HOME/bin:$PATH"
fi

# === Python ===
if [ -d "$HOME/Library/Python/3.9" ]; then
    export PYTHON_HOME="$HOME/Library/Python/3.9"
    export PATH="$PYTHON_HOME/bin:$PATH"
fi

# === miniforge3 (conda/mamba) ===
if [ -d "$HOME/miniforge3" ]; then
    export MINIFORGE3_HOME="$HOME/miniforge3"
    export PATH="$MINIFORGE3_HOME/bin:$PATH"
fi

if [ -f "$HOME/.conda_init.sh" ]; then
    source "$HOME/.conda_init.sh"
fi

# === Conda 别名 ===
alias c0='conda deactivate'
alias c1='conda activate asmemorysrv'
alias c2='conda activate agent0'

# === Node.js (NVM) ===
export NVM_DIR="$HOME/.nvm"
[ -s "$NVM_DIR/nvm.sh" ] && \. "$NVM_DIR/nvm.sh"
[ -s "$NVM_DIR/bash_completion" ] && \. "$NVM_DIR/bash_completion"

# === C++ / LLVM ===
export CFLAGS="$CFLAGS -I/opt/homebrew/opt/llvm/include"
export CPPFLAGS="$CPPFLAGS -I/usr/local/opt/llvm/include"
export LDFLAGS="$LDFLAGS -L/usr/local/opt/llvm/lib"
export PATH="/usr/local/opt/llvm/bin:$PATH"

# === OrbStack ===
# d1: egg-dev (Linux Machine, arm64 原生, 通用 C++ 开发)
# d2: vsag-dev (Docker Container, amd64 QEMU, VSAG 专用)
alias d1='ssh egg-dev@orb'
alias d2='docker exec -it -w /root/project/vsag vsag-dev zsh'

fi  # end macOS only — 开发
PROFILE_EOF

echo ">>> ~/.profile 生成完成"
