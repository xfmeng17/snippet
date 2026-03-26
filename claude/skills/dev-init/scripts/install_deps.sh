#!/bin/bash
# =============================================================================
# 安装基础依赖
# 用法: ./install_deps.sh
# =============================================================================
set -e

OS_TYPE=$(uname -s)
ARCH=$(uname -m)

echo ">>> 安装基础依赖 (OS=$OS_TYPE, ARCH=$ARCH)"

if [ "$OS_TYPE" = "Linux" ]; then
    # Linux: 使用 apt
    if command -v apt-get &>/dev/null; then
        echo ">>> 使用 apt-get 安装..."
        apt-get update
        apt-get install -y \
            zsh git vim curl wget jq \
            build-essential cmake \
            python3 python3-pip \
            clang-format clang-tidy
        # C++ 静态分析: cpplint (pip 安装)
        python3 -m pip install --break-system-packages cpplint 2>/dev/null || \
            python3 -m pip install cpplint
    elif command -v yum &>/dev/null; then
        echo ">>> 使用 yum 安装..."
        yum install -y \
            zsh git vim curl wget jq \
            gcc gcc-c++ make cmake \
            python3 python3-pip \
            clang-tools-extra
        # C++ 静态分析: cpplint (pip 安装)
        python3 -m pip install --break-system-packages cpplint 2>/dev/null || \
            python3 -m pip install cpplint
    else
        echo "!!! 未识别的包管理器，请手动安装: zsh git vim curl wget jq build-essential cmake python3"
        exit 1
    fi

elif [ "$OS_TYPE" = "Darwin" ]; then
    # macOS: 使用 Homebrew
    if ! command -v brew &>/dev/null; then
        echo ">>> 安装 Homebrew..."
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
        # 加载 brew 到当前 session
        if [ "$ARCH" = "arm64" ]; then
            eval "$(/opt/homebrew/bin/brew shellenv)"
        else
            eval "$(/usr/local/bin/brew shellenv)"
        fi
    fi
    echo ">>> 使用 Homebrew 安装..."
    brew install git curl wget jq cmake python3
else
    echo "!!! 不支持的操作系统: $OS_TYPE"
    exit 1
fi

echo ">>> 基础依赖安装完成"
