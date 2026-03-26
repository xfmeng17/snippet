---
name: gcp
description: Google-style Conventional Commits + push to remote. Use when user wants to commit and push changes.
allowed-tools: Bash(git *)
---

# Git Commit & Push (Google Conventional Commits Style)

## 工作流程

1. 运行 `git status` 查看变更文件（不要用 -uall）
2. 运行 `git diff` 和 `git diff --cached` 查看具体改动
3. 运行 `git log --oneline -5` 查看最近提交风格
4. 分析所有变更，生成 Conventional Commits 格式的提交信息
5. 用 `git add` 添加相关文件（**不要用 `git add -A` 或 `git add .`**，逐个添加）
6. 用 `git commit` 提交
7. 用 `git push` 推送到远端

## Commit Message 格式

```
<type>(<scope>): <subject>

<body>

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>
```

### Type 类型

| type | 含义 |
|------|------|
| feat | 新功能 |
| fix | 修复 bug |
| docs | 文档变更 |
| style | 格式调整（不影响逻辑） |
| refactor | 重构（不新增功能也不修 bug） |
| test | 测试相关 |
| chore | 构建、依赖、工具链变更 |
| perf | 性能优化 |

### 规则

- **subject**: 祈使语气、小写开头、不加句号、不超过 50 字符
- **scope**: 可选，表示影响范围（如模块名、组件名）
- **body**: 可选，解释 why 而非 what，72 字符换行
- **破坏性变更**: 用 `feat!:` 或 footer 加 `BREAKING CHANGE:`
- **不要提交**含密钥的文件（.env, credentials 等）
- commit message 通过 HEREDOC 传递以确保格式正确

### 示例

```
feat(lz4): add Bazel build rules for LZ4 compression library

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>
```

```
fix(leveldb): resolve macOS platform detection in BUILD file

Add _DARWIN_C_SOURCE define to fix compilation on macOS 15.

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>
```

```
chore: remove broken oneTBB, jemalloc, and RocksDB build rules

These libraries have compatibility issues with current toolchains:
- oneTBB: gcc 13 requires -mwaitpkg for _tpause instruction
- jemalloc: needs autoconf-generated headers, pure cc_library insufficient
- RocksDB: BUILD file missing v10.5.1 source files

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>
```

## Push 规则

- 推送到当前分支对应的远端分支
- 如果没有上游分支，用 `git push -u origin <branch>`
- **禁止** `--force` 推送到 main/master
- 推送前确认当前分支名，推送后显示结果
