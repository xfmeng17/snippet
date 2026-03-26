# ccuse — Claude Code Provider 切换器

一个 30 行 shell 函数，零依赖，秒级切换 Claude Code 的 API Provider。

## 为什么不用 cc-switch？

[cc-switch](https://github.com/nicholascelesworthy/cc-switch) 是目前业内比较流行的 Claude Code provider 切换方案，但我觉得它太重了：

| | cc-switch | ccuse |
|---|---|---|
| 实现方式 | Node.js CLI，起一个本地代理端口 | 30 行 shell 函数 |
| 依赖 | Node.js + npm install | 无（POSIX shell） |
| 切换原理 | 本地 proxy 转发请求 | 直接替换 `settings.json` |
| 运行时开销 | 常驻进程 + 端口监听 | 零（切完即走） |
| 切换速度 | 需要重启代理 | 瞬间（一次 `cp`） |

**ccuse 的设计哲学：Claude Code 已经支持通过 `settings.json` 里的环境变量来配置 provider，那直接切文件就好了，不需要中间层。**

## 原理

```
~/.claude/
├── settings.json            # Claude Code 读取的配置（活跃配置）
├── settings_zenmux.json     # Provider A 的完整配置
├── settings_minimax.json    # Provider B 的完整配置
└── settings_glm.json        # Provider C 的完整配置
```

- 每个 `settings_<name>.json` 是一份完整的 Claude Code 配置，包含该 provider 的 API key、base URL、模型名等
- `ccuse <name>` 就是把 `settings_<name>.json` 复制为 `settings.json`
- 每个配置文件的 `env` 中带 `CCUSE_PROVIDER` 字段标识自身名称，用于无参调用时显示当前 provider

## 安装

把 `ccuse.sh` 中的函数复制到你的 `~/.profile`（或 `~/.bashrc` / `~/.zshrc`）中，然后 source 即可：

```bash
# 方式一：直接 source（推荐）
cat ccuse.sh >> ~/.profile
source ~/.profile

# 方式二：或者在 shell 配置中 source 此文件
echo 'source /path/to/ccuse.sh' >> ~/.profile
source ~/.profile
```

## 配置 Provider

为每个 provider 创建一个 `~/.claude/settings_<name>.json`，参考 `examples/` 目录下的示例。

关键字段说明：

```jsonc
{
  "env": {
    "ANTHROPIC_AUTH_TOKEN": "your-api-key",      // API 密钥
    "ANTHROPIC_BASE_URL": "https://xxx/anthropic", // Provider 的 API 端点
    "ANTHROPIC_MODEL": "claude-opus-4-6",         // 默认模型
    "ANTHROPIC_DEFAULT_HAIKU_MODEL": "...",       // haiku 模型（/haiku 切换用）
    "ANTHROPIC_DEFAULT_OPUS_MODEL": "...",        // opus 模型（/opus 切换用）
    "ANTHROPIC_DEFAULT_SONNET_MODEL": "...",      // sonnet 模型（/sonnet 切换用）
    "CCUSE_PROVIDER": "your-provider-name"        // ← 必须与文件名中的 <name> 一致
  },
  "model": "claude-opus-4-6"                      // 与 ANTHROPIC_MODEL 保持一致
}
```

> **注意**：不同 provider 的模型 ID 可能不同。例如直连 Anthropic 用 `claude-opus-4-6`，而某些转发服务可能需要 `anthropic/claude-opus-4.6` 带前缀的格式。按照各 provider 的文档填写即可。

## 使用

```bash
# 查看当前 provider 和可用列表
$ ccuse
当前 provider: zenmux
    "ANTHROPIC_BASE_URL": "https://zenmux.ai/api/anthropic",
    "ANTHROPIC_MODEL": "anthropic/claude-opus-4.6",

可用 provider:
  * zenmux         https://zenmux.ai/api/anthropic
    minimax        https://api.minimaxi.com/anthropic
    glm            https://open.bigmodel.cn/anthropic

# 切换到 minimax
$ ccuse minimax
已切换到: minimax
    "ANTHROPIC_BASE_URL": "https://api.minimaxi.com/anthropic",
    "ANTHROPIC_MODEL": "MiniMax-M2.7",

# 切换到不存在的 provider 会报错并列出可用项
$ ccuse foo
找不到配置: /home/user/.claude/settings_foo.json
当前 provider: minimax
...
```

切换后，新启动的 `claude` 会话即使用新 provider。已运行的会话不受影响。

## 文件结构

```
ccuse/
├── README.md              # 本文档
├── ccuse.sh               # ccuse 函数源码，复制到 shell 配置中使用
└── examples/
    ├── settings_zenmux.json    # ZenMux provider 示例
    ├── settings_minimax.json   # MiniMax provider 示例
    └── settings_glm.json       # GLM-4 provider 示例
```

## FAQ

**Q: 切换后已有的 claude 会话会受影响吗？**
A: 不会。`settings.json` 只在 claude 启动时读取，已运行的会话使用的是启动时加载的配置。

**Q: 我的 settings.json 里有额外字段（如 `enabledPlugins`），切换会丢失吗？**
A: 会。ccuse 是整文件替换。建议把所有需要的字段都写进每个 `settings_<name>.json` 中。

**Q: 支持 bash 吗？**
A: 支持。ccuse 使用 POSIX 兼容的 shell 语法（`local` 除外，但几乎所有主流 shell 都支持 `local`）。在 bash / zsh / dash 等 shell 中均可使用。

## License

MIT
