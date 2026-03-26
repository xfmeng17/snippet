#!/bin/bash
# =============================================================================
# 生成 Claude Code Provider 配置文件
# API Key 使用占位符，需要用户手动替换
# =============================================================================
set -e

CLAUDE_DIR="$HOME/.claude"
mkdir -p "$CLAUDE_DIR"

# 共享的 statusLine 配置
STATUS_LINE='"statusLine": {
    "command": "jq -r '\''\"\\u001b[0;32m\\(.model.id)\\u001b[0m:\\u001b[0;34m\\(.workspace.current_dir)\\u001b[0m | ctx:\\(.context_window.used_percentage // 0)% \\(((.context_window.total_input_tokens // 0) + (.context_window.total_output_tokens // 0)) / 1000 | floor)k tok | $\\(.cost.total_cost_usd // 0 | . * 100 | round / 100)\"'\''",
    "type": "command"
  }'

echo ">>> 生成 Claude Code Provider 配置..."

# --- ZenMux ---
cat > "$CLAUDE_DIR/settings_zenmux.json" << 'EOF'
{
  "env": {
    "ANTHROPIC_AUTH_TOKEN": "<ZENMUX_API_KEY>",
    "ANTHROPIC_BASE_URL": "https://zenmux.ai/api/anthropic",
    "ANTHROPIC_MODEL": "anthropic/claude-opus-4.6",
    "ANTHROPIC_DEFAULT_HAIKU_MODEL": "anthropic/claude-haiku-4.5",
    "ANTHROPIC_DEFAULT_OPUS_MODEL": "anthropic/claude-opus-4.6",
    "ANTHROPIC_DEFAULT_SONNET_MODEL": "anthropic/claude-sonnet-4.6",
    "CLAUDE_CODE_ATTRIBUTION_HEADER": "0",
    "CLAUDE_CODE_DISABLE_NONESSENTIAL_TRAFFIC": "1"
  },
  "model": "anthropic/claude-opus-4.6",
  "skipWebFetchPreflight": true,
  "statusLine": {
    "command": "jq -r '\"\\u001b[0;32m\\(.model.id)\\u001b[0m:\\u001b[0;34m\\(.workspace.current_dir)\\u001b[0m | ctx:\\(.context_window.used_percentage // 0)% \\(((.context_window.total_input_tokens // 0) + (.context_window.total_output_tokens // 0)) / 1000 | floor)k tok | $\\(.cost.total_cost_usd // 0 | . * 100 | round / 100)\"'",
    "type": "command"
  }
}
EOF
echo "  - settings_zenmux.json"

# --- Timicc Claude ---
cat > "$CLAUDE_DIR/settings_timicc_claude.json" << 'EOF'
{
  "env": {
    "ANTHROPIC_AUTH_TOKEN": "<TIMICC_CLAUDE_API_KEY>",
    "ANTHROPIC_BASE_URL": "https://timicc.cc",
    "ANTHROPIC_MODEL": "claude-opus-4-6",
    "ANTHROPIC_DEFAULT_HAIKU_MODEL": "claude-haiku-4-5-20251001",
    "ANTHROPIC_DEFAULT_OPUS_MODEL": "claude-opus-4-6",
    "ANTHROPIC_DEFAULT_SONNET_MODEL": "claude-sonnet-4-6",
    "CLAUDE_CODE_ATTRIBUTION_HEADER": "0",
    "CLAUDE_CODE_DISABLE_NONESSENTIAL_TRAFFIC": "1"
  },
  "model": "claude-opus-4-6",
  "skipWebFetchPreflight": true,
  "statusLine": {
    "command": "jq -r '\"\\u001b[0;32m\\(.model.id)\\u001b[0m:\\u001b[0;34m\\(.workspace.current_dir)\\u001b[0m | ctx:\\(.context_window.used_percentage // 0)% \\(((.context_window.total_input_tokens // 0) + (.context_window.total_output_tokens // 0)) / 1000 | floor)k tok | $\\(.cost.total_cost_usd // 0 | . * 100 | round / 100)\"'",
    "type": "command"
  }
}
EOF
echo "  - settings_timicc_claude.json"

# --- Timicc GPT ---
cat > "$CLAUDE_DIR/settings_timicc_gpt.json" << 'EOF'
{
  "env": {
    "ANTHROPIC_AUTH_TOKEN": "<TIMICC_GPT_API_KEY>",
    "ANTHROPIC_BASE_URL": "https://timicc.cc",
    "ANTHROPIC_MODEL": "gpt-5.4",
    "ANTHROPIC_DEFAULT_HAIKU_MODEL": "gpt-5.2-codex",
    "ANTHROPIC_DEFAULT_OPUS_MODEL": "gpt-5.4",
    "ANTHROPIC_DEFAULT_SONNET_MODEL": "gpt-5.3-codex",
    "CLAUDE_CODE_ATTRIBUTION_HEADER": "0",
    "CLAUDE_CODE_DISABLE_NONESSENTIAL_TRAFFIC": "1"
  },
  "model": "gpt-5.4",
  "skipWebFetchPreflight": true,
  "statusLine": {
    "command": "jq -r '\"\\u001b[0;32m\\(.model.id)\\u001b[0m:\\u001b[0;34m\\(.workspace.current_dir)\\u001b[0m | ctx:\\(.context_window.used_percentage // 0)% \\(((.context_window.total_input_tokens // 0) + (.context_window.total_output_tokens // 0)) / 1000 | floor)k tok | $\\(.cost.total_cost_usd // 0 | . * 100 | round / 100)\"'",
    "type": "command"
  }
}
EOF
echo "  - settings_timicc_gpt.json"

# --- MiniMax ---
cat > "$CLAUDE_DIR/settings_minimax.json" << 'EOF'
{
  "env": {
    "ANTHROPIC_AUTH_TOKEN": "<MINIMAX_API_KEY>",
    "ANTHROPIC_BASE_URL": "https://api.minimaxi.com/anthropic",
    "ANTHROPIC_MODEL": "MiniMax-M2.7",
    "ANTHROPIC_DEFAULT_HAIKU_MODEL": "MiniMax-M2.7",
    "ANTHROPIC_DEFAULT_OPUS_MODEL": "MiniMax-M2.7",
    "ANTHROPIC_DEFAULT_SONNET_MODEL": "MiniMax-M2.7",
    "CLAUDE_CODE_ATTRIBUTION_HEADER": "0",
    "CLAUDE_CODE_DISABLE_NONESSENTIAL_TRAFFIC": "1"
  },
  "model": "MiniMax-M2.7",
  "skipWebFetchPreflight": true,
  "statusLine": {
    "command": "jq -r '\"\\u001b[0;32m\\(.model.id)\\u001b[0m:\\u001b[0;34m\\(.workspace.current_dir)\\u001b[0m | ctx:\\(.context_window.used_percentage // 0)% \\(((.context_window.total_input_tokens // 0) + (.context_window.total_output_tokens // 0)) / 1000 | floor)k tok | $\\(.cost.total_cost_usd // 0 | . * 100 | round / 100)\"'",
    "type": "command"
  }
}
EOF
echo "  - settings_minimax.json"

echo ""
echo ">>> Provider 配置生成完成"
echo ">>> 请编辑以上文件，将 <XXX_API_KEY> 替换为真实的 API Key"
echo ">>> 然后运行 'ccuse <provider>' 激活"
