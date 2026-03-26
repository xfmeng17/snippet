# gcp (Git Commit & Push)

Google Conventional Commits 风格的 Git 提交 + 推送 Claude Code Skill。

## 功能

- 自动分析变更，生成 Conventional Commits 格式的 commit message
- 逐个 `git add` 文件（不用 `git add .`）
- 自动推送到远端

## 安装

```bash
cp -r gcp ~/.claude/skills/
```

## 使用

在 Claude Code 中：

```
/gcp
```

## Commit 格式

```
<type>(<scope>): <subject>

<body>
```

支持的 type: `feat`, `fix`, `docs`, `style`, `refactor`, `test`, `chore`, `perf`
