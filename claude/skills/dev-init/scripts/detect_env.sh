#!/bin/bash
# =============================================================================
# 环境检测脚本 — 输出当前系统信息
# =============================================================================

OS_TYPE=$(uname -s)
ARCH=$(uname -m)
IS_DOCKER=false
[ -f /.dockerenv ] && IS_DOCKER=true

echo "========== 环境检测 =========="
echo "OS:       $OS_TYPE"
echo "架构:     $ARCH"
echo "Docker:   $IS_DOCKER"
echo "用户:     $(whoami)"
echo "Home:     $HOME"
echo "当前Shell: $SHELL"
echo "zsh路径:  $(which zsh 2>/dev/null || echo '未安装')"
echo "git:      $(which git 2>/dev/null || echo '未安装')"
echo "curl:     $(which curl 2>/dev/null || echo '未安装')"
echo "jq:       $(which jq 2>/dev/null || echo '未安装')"
echo "vim:      $(which vim 2>/dev/null || echo '未安装')"

if [ "$OS_TYPE" = "Darwin" ]; then
    echo "Homebrew: $(which brew 2>/dev/null || echo '未安装')"
fi

echo "Oh-My-Zsh: $([ -d "$HOME/.oh-my-zsh" ] && echo '已安装' || echo '未安装')"
echo "Claude:   $([ -d "$HOME/.claude" ] && echo '目录存在' || echo '目录不存在')"
echo "=============================="
