# ccuse — Claude Code Provider Switcher

**English** | [中文](README_CN.md)

A 30-line shell function to instantly switch Claude Code API providers. Zero dependencies.

## Why not cc-switch?

[cc-switch](https://github.com/nicholascelesworthy/cc-switch) is a popular Claude Code provider switching solution, but it's heavier than necessary:

| | cc-switch | ccuse |
|---|---|---|
| Implementation | Node.js CLI, spawns a local proxy on a port | 30-line shell function |
| Dependencies | Node.js + npm install | None (POSIX shell) |
| Switching mechanism | Local proxy forwarding requests | Direct `settings.json` replacement |
| Runtime overhead | Persistent process + port listener | Zero (fire and forget) |
| Switch speed | Requires proxy restart | Instant (single `cp`) |

**Design philosophy: Claude Code already supports provider configuration via environment variables in `settings.json` — just swap the file, no middleware needed.**

## How it works

```
~/.claude/
├── settings.json            # Active config (read by Claude Code)
├── settings_zenmux.json     # Provider A config
├── settings_minimax.json    # Provider B config
└── settings_glm.json        # Provider C config
```

- Each `settings_<name>.json` is a complete Claude Code config with the provider's API key, base URL, model names, etc.
- `ccuse <name>` copies `settings_<name>.json` to `settings.json`
- Each config file contains a `CCUSE_PROVIDER` field in `env` to identify itself, used by the no-argument display

## Installation

Copy the function from `ccuse.sh` into your `~/.profile` (or `~/.bashrc` / `~/.zshrc`), then source it:

```bash
# Option 1: Append and source (recommended)
cat ccuse.sh >> ~/.profile
source ~/.profile

# Option 2: Source the file directly
echo 'source /path/to/ccuse.sh' >> ~/.profile
source ~/.profile
```

## Provider configuration

Create a `~/.claude/settings_<name>.json` for each provider. See `examples/` for references.

Key fields:

```jsonc
{
  "env": {
    "ANTHROPIC_AUTH_TOKEN": "your-api-key",        // API key
    "ANTHROPIC_BASE_URL": "https://xxx/anthropic",  // Provider endpoint
    "ANTHROPIC_MODEL": "claude-opus-4-6",           // Default model
    "ANTHROPIC_DEFAULT_HAIKU_MODEL": "...",         // For /haiku switching
    "ANTHROPIC_DEFAULT_OPUS_MODEL": "...",          // For /opus switching
    "ANTHROPIC_DEFAULT_SONNET_MODEL": "...",        // For /sonnet switching
    "CCUSE_PROVIDER": "your-provider-name"          // ← Must match <name> in filename
  },
  "model": "claude-opus-4-6"                        // Keep in sync with ANTHROPIC_MODEL
}
```

> **Note**: Model IDs vary across providers. Direct Anthropic uses `claude-opus-4-6`, while some proxy services require prefixed formats like `anthropic/claude-opus-4.6`. Follow your provider's documentation.

## Usage

```bash
# Show current provider and available list
$ ccuse
Current provider: zenmux
    "ANTHROPIC_BASE_URL": "https://zenmux.ai/api/anthropic",
    "ANTHROPIC_MODEL": "anthropic/claude-opus-4.6",

Available providers:
  * zenmux         https://zenmux.ai/api/anthropic
    minimax        https://api.minimaxi.com/anthropic
    glm            https://open.bigmodel.cn/anthropic

# Switch to minimax
$ ccuse minimax
Switched to: minimax
    "ANTHROPIC_BASE_URL": "https://api.minimaxi.com/anthropic",
    "ANTHROPIC_MODEL": "MiniMax-M2.7",

# Non-existent provider shows error and lists available ones
$ ccuse foo
Config not found: /home/user/.claude/settings_foo.json
Current provider: minimax
...
```

After switching, new `claude` sessions use the new provider. Running sessions are unaffected.

## File structure

```
ccuse/
├── README.md              # English documentation
├── README_CN.md           # Chinese documentation
├── ccuse.sh               # Source code — copy into your shell config
└── examples/
    ├── settings_zenmux.json
    ├── settings_minimax.json
    └── settings_glm.json
```

## FAQ

**Q: Does switching affect running claude sessions?**
A: No. `settings.json` is only read at startup. Running sessions keep their original config.

**Q: Will extra fields in settings.json (e.g. `enabledPlugins`) be lost?**
A: Yes. ccuse does a full file replacement. Include all needed fields in each `settings_<name>.json`.

**Q: Does it work with bash?**
A: Yes. ccuse uses POSIX-compatible shell syntax (`local` is the only exception, but virtually all mainstream shells support it). Works in bash / zsh / dash.

## License

MIT
