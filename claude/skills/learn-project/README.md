**English** | [中文](README_CN.md)

# learn-project

A general-purpose Claude Code Skill for learning open-source projects and codebases.

## Three Learning Modes

| Mode | Command Example | Output |
|------|----------------|--------|
| **notes** — Source Code Notes | `/learn-project notes ~/project/redis` | README + topic-based notes |
| **course** — Lesson-based Course | `/learn-project course ~/project/leveldb` | README + per-lesson documents |
| **mini** — Build a Mini Version | `/learn-project mini ~/project/sqlite ./nano-db` | TASK.md + sub-task PRDs + architecture docs |

## Installation

Copy the `learn-project/` directory into `~/.claude/skills/`:

```bash
cp -r learn-project ~/.claude/skills/
```

## Usage

In Claude Code:

```
/learn-project course ~/project/leveldb
```

Or use natural language: "Help me learn the SSTable implementation in LevelDB"

## Highlights

- **Context Decay Protection**: The course mode uses a `.course_log.md` mechanism to maintain quality in later lessons
- **Pitfall Records**: Includes common issues and fixes summarized from real project reviews
- **Three Templates**: Detailed document templates for notes/course/mini modes

## File Structure

```
learn-project/
├── SKILL.md                        # Skill definition
└── references/
    ├── template_notes.md           # Source code notes template
    ├── template_course.md          # Course lesson template
    └── template_mini.md            # Mini version template
```
