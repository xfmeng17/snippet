**English** | [中文](README_CN.md)

# snippet

Personal technical learning repository: code snippets + technical survey documents.

These snippets are not simplified versions or toy demos — the goal is **a rewrite that is equally complete yet more readable with cleaner coding conventions**. Reading the code is enough to learn the core engineering decisions of the original library.

## Learning Approach

1. **Topic Selection**: Pick core modules worth deep-diving into from well-known C++ projects
2. **Rewrite**: Rewrite in pure STL, remove framework dependencies, preserve the full algorithm logic
3. **Documentation**: Each module comes with a numerical walkthrough document (concrete numbers, hand-verifiable)
4. **Testing**: Thorough unit tests + integration tests + concurrency tests

## Project List

| Directory | Type | Topic | Description |
|-----------|------|-------|-------------|
| [brpc_lalb](brpc_lalb/) | Rewrite | Locality-Aware Load Balancing | Extracted from [Apache brpc](https://github.com/apache/brpc), Bazel, C++17 |
| [concurrent_hashmap](concurrent_hashmap/) | Survey | C++ Concurrent HashMap | Industry solution comparison: from read wait-free + write shard-lock to fully lock-free |
| [zenmux](zenmux/) | Survey | ZenMux LLM API Provider | Pricing model explained (subscription Flow mechanism, PAYG, Claude API billing comparison) |
| [ccuse](ccuse/) | Tool | Claude Code Provider Switcher | 30-line shell function, zero-dependency instant provider switching, replaces cc-switch |
| [claude](claude/) | Tool | Claude Code Custom Skills | learn-project, cpp-style-check, dev-init, gcp, and other custom skills |
