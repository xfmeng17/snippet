#!/bin/bash
# =============================================================================
# 一键初始化开发环境
# 用法: bash init.sh [--all | --deps | --zsh | --profile | --providers | --shell]
# 不带参数等同于 --all
# =============================================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

run_step() {
    echo ""
    echo "============================================================"
    echo "  $1"
    echo "============================================================"
    bash "$SCRIPT_DIR/$2"
}

show_summary() {
    echo ""
    echo "===================================="
    echo "  开发环境初始化完成!"
    echo "===================================="
    echo ""
    echo "已配置:"
    echo "  - [x] ~/.zshrc (Oh My Zsh + fletcherm 主题 + 插件)"
    echo "  - [x] ~/.profile (别名、PATH、工具链、ccuse 函数)"
    echo "  - [x] Claude Code Provider 配置 (zenmux / timicc_claude / timicc_gpt / minimax)"
    echo ""
    echo "待手动操作:"
    echo "  1. 编辑 ~/.claude/settings_*.json 填入真实 API Key"
    echo "  2. 运行 ccuse <provider> 激活一个 provider"
    echo "  3. (可选) 安装 miniforge3: https://github.com/conda-forge/miniforge"
    echo "  4. (可选) 安装 NVM: https://github.com/nvm-sh/nvm"
    echo "  5. 重新打开终端或执行: source ~/.zshrc"
    echo ""
    echo "提示:"
    echo "  ccuse          查看所有可用 provider"
    echo "  ccuse zenmux   切换到 ZenMux provider"
    echo "  gdd foo.cc     快速编译 debug 版 C++ 程序"
    echo ""
}

# 解析参数
ACTION="${1:---all}"

case "$ACTION" in
    --all)
        run_step "1/6 环境检测" "detect_env.sh"
        run_step "2/6 安装基础依赖" "install_deps.sh"
        run_step "3/6 安装 Oh My Zsh" "install_ohmyzsh.sh"
        run_step "4/6 生成 ~/.zshrc" "gen_zshrc.sh"
        run_step "5/6 生成 ~/.profile" "gen_profile.sh"
        run_step "6/6 生成 Provider 配置" "gen_providers.sh"
        show_summary
        ;;
    --deps)
        run_step "安装基础依赖" "install_deps.sh"
        ;;
    --zsh)
        run_step "安装 Oh My Zsh" "install_ohmyzsh.sh"
        run_step "生成 ~/.zshrc" "gen_zshrc.sh"
        ;;
    --profile)
        run_step "生成 ~/.profile" "gen_profile.sh"
        ;;
    --providers)
        run_step "生成 Provider 配置" "gen_providers.sh"
        ;;
    --shell)
        run_step "设置默认 Shell" "setup_shell.sh"
        ;;
    --help|-h)
        echo "用法: bash init.sh [选项]"
        echo ""
        echo "选项:"
        echo "  --all        执行全部步骤 (默认)"
        echo "  --deps       仅安装基础依赖"
        echo "  --zsh        仅安装 Oh My Zsh + 生成 .zshrc"
        echo "  --profile    仅生成 .profile"
        echo "  --providers  仅生成 Claude Code Provider 配置"
        echo "  --shell      仅设置 zsh 为默认 shell"
        echo "  --help       显示帮助"
        ;;
    *)
        echo "未知选项: $ACTION"
        echo "运行 'bash init.sh --help' 查看帮助"
        exit 1
        ;;
esac
