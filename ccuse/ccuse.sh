# ccuse: Claude Code Provider 切换
# 用法: ccuse <provider>  切换到指定 provider
#       ccuse             显示当前 provider 和可用列表
#
# 安装: 将此函数复制到 ~/.profile (或 ~/.bashrc / ~/.zshrc)，然后 source 即可。
# 配置: 为每个 provider 创建 ~/.claude/settings_<name>.json，
#       其中 env.CCUSE_PROVIDER 字段值与 <name> 一致。
ccuse() {
    local claude_dir="$HOME/.claude"
    if [ -z "$1" ]; then
        local current=$(grep 'CCUSE_PROVIDER' "$claude_dir/settings.json" 2>/dev/null | sed 's/.*: *"//;s/".*//')
        echo "当前 provider: \033[1;32m${current:-未知}\033[0m"
        grep -E 'BASE_URL|_MODEL"' "$claude_dir/settings.json" 2>/dev/null | head -4
        echo "\n可用 provider:"
        for f in "$claude_dir"/settings_*.json; do
            local name=$(basename "$f" .json | sed 's/settings_//')
            local url=$(grep 'BASE_URL' "$f" | head -1 | sed 's/.*: *"//;s/".*//')
            local marker="  "
            [ "$name" = "$current" ] && marker="* "
            printf "  %s%-14s %s\n" "$marker" "$name" "$url"
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
