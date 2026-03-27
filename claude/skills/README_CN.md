[English](README.md) | **中文**

# Claude Code Skills

一组实用的 [Claude Code](https://docs.anthropic.com/en/docs/claude-code) 自定义 Skill，覆盖代码学习、C++ 风格检查、Git 提交和开发环境初始化。

## Skills 一览

| Skill | 说明 | 通用性 |
|-------|------|--------|
| [learn-project](learn-project/) | 学习开源项目：源码笔记 / 分课时课程 / 手写 Mini 版 | 通用 |
| [learn-vec](learn-vec/) | 学习向量检索/ANN 算法：课程 / 手写 mini / 源码笔记 / 水平诊断 / 教学互动（主动回忆、费曼技巧） | 领域专用 |
| [cpp-style-check](cpp-style-check/) | C++ 代码风格检查（Google Style + 认知复杂度 + 自定义规则） | 通用 |
| [gcp](gcp/) | Google Conventional Commits 风格的 Git 提交 + 推送 | 通用 |
| [dev-init](dev-init/) | 开发环境一键初始化（zsh + Oh My Zsh + Provider 配置） | 个人参考 |

## 安装

将需要的 skill 目录复制到 `~/.claude/skills/` 下即可：

```bash
# 安装单个 skill
cp -r learn-project ~/.claude/skills/

# 安装全部
cp -r learn-project learn-vec cpp-style-check gcp dev-init ~/.claude/skills/
```

## 什么是 Claude Code Skill

Skill 是 Claude Code 的自定义指令扩展。每个 skill 目录下有一个 `SKILL.md` 文件，定义了：

- **触发条件**：什么时候激活（斜杠命令或自然语言）
- **工作流程**：Claude 执行的具体步骤
- **工具权限**：允许使用哪些工具（Bash、Read、Write 等）

详见 [Claude Code 文档](https://docs.anthropic.com/en/docs/claude-code)。

## 各 Skill 简介

### learn-project — 开源项目深度学习

三种模式帮你系统性地理解一个项目的 How、Why、Trade-off：

- **notes**：按主题整理源码笔记
- **course**：从零开始的分课时递进式课程（含 context 腐蚀防护机制）
- **mini**：通过手写简化版来理解原项目

```
/learn-project course ~/project/leveldb
```

### learn-vec — 向量检索/ANN 算法深度学习

learn-project 的向量检索领域专用扩展。五种模式：

- **course**：系统性分课时课程（含 context 腐蚀防护）
- **mini**：从零手写简化版 ANN 实现
- **notes**：整理向量检索库的源码笔记
- **diagnose**：5 题水平诊断（L0–L3），自动调整课程难度
- **quiz / explain**：主动回忆测试 + 费曼技巧讲解

内置领域知识：算法族谱、常见误区、Benchmark 方法论、标准数据集。

```
/learn-vec course ~/project/hnswlib ./hnsw_learn/
/learn-vec diagnose
/learn-vec quiz hnsw-search
/learn-vec explain "乘积量化"
```

### cpp-style-check — C++ 代码风格检查

四层检查：clang-format → cpplint → clang-tidy（认知复杂度） → 自定义规则。

自定义规则包括：禁止异常、限制 auto、强制大括号、禁止抛异常的数值解析函数。

```
/cpp-style-check src/my_module.cc
```

### gcp — Git Commit & Push

分析变更，自动生成 Conventional Commits 格式的 commit message 并推送。

```
/gcp
```

### dev-init — 开发环境初始化

> **注意**：这是个人配置的参考实现，使用前请根据自己的需求修改脚本。

一键初始化 zsh 开发环境：依赖安装、Oh My Zsh、.zshrc/.profile 生成、Claude Code Provider 配置。

```
/dev-init
```

## License

MIT
