[English](README.md) | **中文**

# learn-project

学习开源项目/代码库的通用 Claude Code Skill。

## 三种学习模式

| 模式 | 命令示例 | 产出 |
|------|---------|------|
| **notes** — 源码笔记 | `/learn-project notes ~/project/redis` | README + 分主题笔记 |
| **course** — 分课时课程 | `/learn-project course ~/project/leveldb` | README + 分课时讲解文档 |
| **mini** — 手写 Mini 版 | `/learn-project mini ~/project/sqlite ./nano-db` | TASK.md + 子任务 PRD + 架构文档 |

## 安装

将 `learn-project/` 目录复制到 `~/.claude/skills/` 下：

```bash
cp -r learn-project ~/.claude/skills/
```

## 使用

在 Claude Code 中：

```
/learn-project course ~/project/leveldb
```

或自然语言："帮我学习 LevelDB 的 SSTable 实现"

## 特色

- **Context 腐蚀防护**：course 模式通过 `.course_log.md` 机制保持后半段课时质量
- **踩坑记录**：内含从实际项目 Review 中总结的常见问题和修正方案
- **三种模板**：notes/course/mini 各有详细的文档模板

## 文件结构

```
learn-project/
├── SKILL.md                        # Skill 定义
└── references/
    ├── template_notes.md           # 源码笔记模板
    ├── template_course.md          # 课程讲解模板
    └── template_mini.md            # 手写 Mini 版模板
```
