**English** | [中文](README_CN.md)

# gcp (Git Commit & Push)

A Claude Code Skill for Git commit and push using the Conventional Commits format.

## Features

- Automatically analyzes changes and generates Conventional Commits formatted commit messages
- Adds files individually with `git add` (avoids `git add .`)
- Automatically pushes to the remote

## Installation

```bash
cp -r gcp ~/.claude/skills/
```

## Usage

In Claude Code:

```
/gcp
```

## Commit Format

```
<type>(<scope>): <subject>

<body>
```

Supported types: `feat`, `fix`, `docs`, `style`, `refactor`, `test`, `chore`, `perf`
