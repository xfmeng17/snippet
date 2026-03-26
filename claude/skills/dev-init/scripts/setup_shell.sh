#!/bin/bash
# =============================================================================
# 设置 zsh 为默认 shell
# =============================================================================

ZSH_PATH=$(which zsh 2>/dev/null)

if [ -z "$ZSH_PATH" ]; then
    echo "!!! zsh 未安装，请先运行 install_deps.sh"
    exit 1
fi

if [ "$SHELL" = "$ZSH_PATH" ]; then
    echo ">>> 当前 shell 已经是 zsh，无需修改"
    exit 0
fi

echo ">>> 当前 shell: $SHELL"
echo ">>> zsh 路径: $ZSH_PATH"

IS_DOCKER=false
[ -f /.dockerenv ] && IS_DOCKER=true

if [ "$IS_DOCKER" = "true" ]; then
    echo ">>> Docker 环境，直接修改 /etc/passwd"
    sed -i "s|$(whoami):[^:]*$|$(whoami):$ZSH_PATH|" /etc/passwd
    echo ">>> 已将默认 shell 修改为 zsh"
else
    echo ">>> 执行 chsh -s $ZSH_PATH"
    chsh -s "$ZSH_PATH"
    echo ">>> 已将默认 shell 修改为 zsh，重新登录后生效"
fi
