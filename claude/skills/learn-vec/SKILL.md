---
name: learn-vec
version: "2.0.0"
description: 学习向量检索引擎/ANN 算法的专用技能，支持五种模式：课程(course)、手写 mini(mini)、源码笔记(notes)、水平诊断(diagnose)、教学互动(quiz/explain)。当用户说"学习向量检索"、"学习 ANN 算法"、"learn-vec"时触发。支持 "/learn-vec export/导出" 和 "/learn-vec import/导入"。
---

# Learn Vec — 向量引擎深度学习技能

> **Version**: 2.0.0

专注于向量检索引擎/ANN 算法的深度学习技能。不只是"浏览代码"，而是让你真正学会一个算法/架构的 How、Why、Trade-off。

集成主动回忆（Active Recall）、费曼技巧（Feynman Technique）、苏格拉底追问（Socratic Questioning）、水平诊断（Level Diagnosis）等教育学方法，从"生成课件"进化为"辅助深度学习"。

**与 learn-project 的区别**: learn-vec 内置了向量检索领域知识（算法族谱、常见误区、benchmark 方法论），学生不需要额外准备领域背景。

## 触发条件

- `/learn-vec`
- `/learn-vec [mode] [course_or_topic] {output_path}`
  - 如: `/learn-vec course ~/project/hnswlib ./hnsw_learn/`
  - 如: `/learn-vec mini faiss-ivfpq ./mini_ivfpq/`
  - 如: `/learn-vec notes ~/project/faiss ./faiss_notes/`
  - 如: `/learn-vec diagnose`
  - 如: `/learn-vec quiz hnsw-search`
  - 如: `/learn-vec explain "乘积量化"`

**参数约定**（均可省略，省略时交互确认）:
- `$0` / `mode`: `course` | `mini` | `notes` | `diagnose` | `quiz` | `explain`
- `$1` / `course_or_topic`: 课程名、算法/主题、或概念名
- `$2` / `output_path`: 输出目录（diagnose/quiz/explain 模式不需要）

## 五种学习模式

| 模式 | 适用场景 | 产出 | 周期 |
|------|---------|------|------|
| **A: course** | 从头系统学一个算法或引擎 | README 大纲 + 分课时 md | 1天+ |
| **B: mini** | 从零手写 mini 版理解核心 | TASK.md + tasks/ + docs/ + 代码 | 几天+ |
| **C: notes** | 快速整理一个库的源码 | README + 分主题 md | 半天+ |
| **D: diagnose** | 诊断当前水平，制定路径 | 诊断报告 + 学习建议 | 15min |
| **E: quiz/explain** | 学后巩固，发现盲点 | 互动对话 + 评估报告 | 30min |

## 工作流程

```
用户输入（自然语言 或 /learn-vec [args]）
  │
  ├─ 1. 确认参数（解析 $ARGUMENTS 或交互输入）
  │     模式、源码/主题、输出目录
  │     如果用户没指定模式，根据输入推断后用 AskUserQuestion 确认
  │
  ├─ 2. [可选] 水平诊断（diagnose 模式 或用户首次使用）
  │     5 题快速诊断 → 判定 L0-L3 → 推荐学习路径
  │     诊断结果影响后续 course 的课时跳过策略
  │
  ├─ 3. 学习/了解领域（如有源码目标）
  │     用 Explore agent (very thorough) 按标准源码探索协议:
  │     全局视图 → 入口点定位 → 核心数据结构 → 热路径追踪 → 工程细节
  │     用 WebSearch 搜索论文/技术资料/Benchmark 数据
  │
  ├─ 4. 根据所学到的信息
  │     模式 A: 设计课时大纲（参考推荐编排）
  │     模式 B: 设计 Phase + Task
  │     模式 C: 按模块组织笔记大纲
  │     模式 D: 出诊断题并评估
  │     模式 E: 进入互动对话
  │
  └─ 5. 输出到 output_path（或直接交互）
```

## 模式 A: 课程（course）

**输出结构:**

```
{output_dir}/
├── README.md          # 课程大纲 + 算法速查 + 课时导航
├── .course_log.md     # 跨课时质量锚点（防 context 腐蚀的关键文件）
├── 01_{first_topic}.md
├── 02_{second_topic}.md
├── ...
└── NN_{last_topic}.md
```

**课时内容要求:**
- 每课必须有"直觉"小节（先类比后技术）
- 每课末尾有 2-3 个"常见误区"
- 每课末尾有 3-5 个"思考题（Active Recall）"
- 详细模板见 `references/template_course.md`

### .course_log.md —— 三阶段 Context 管理

**背景**: 课程通常 8-12 课时，后半段生成时 context 已被大量占据，导致质量下降（典型症状: 代码凭印象编、缺少"为什么"、深度骤降）。

**机制**: 在输出目录生成 `.course_log.md`，作为跨课时的"质量锚点"。每完成一课更新一次。

**第 1-4 课时（context 新鲜期）**:
- 正常生成，源码探索结果直接在 context 中
- 第 2-3 课完成后，更新 `.course_log.md` 的"质量标杆"section，提取风格规范

**第 5 课时起（context 膨胀期）**:
- 每个课时用独立的 Agent 生成（`subagent_type: general-purpose`）
- Agent prompt 中包含:
  1. `.course_log.md` 的完整内容（质量标杆 + 源码路径缓存）
  2. 该课时的具体要求（标题、覆盖的源码文件、要讲的知识点）
  3. 前一课时的"本课小结"section（保持衔接）
  4. 明确指令: "用 Read 工具验证所有代码片段和行号"
- Agent 完成后，主 context 只接收产出文件，不接收探索过程
- 更新 `.course_log.md` 的进度行

**最后 1-2 课时（对比/总结课）**:
- 同样用独立 Agent
- 额外传入所有课时的"本课小结"section（通常每课 5-10 行，总共不超过 100 行）

.course_log.md 模板详见 `references/template_course.md`

## 模式 B: 手写 Mini 版（mini）

**输出结构:**

```
{output_dir}/
├── TASK.md                  # 总体规划 (How-Why-Simplify)
├── .claude/CLAUDE.md        # 项目级 Claude 规则/约束
├── tasks/                   # 子任务 PRD
│   ├── task01_{name}.md
│   └── ...
├── docs/
│   ├── architecture.md      # 模块对照（mini vs 原实现）
│   └── algorithm_notes.md   # 算法笔记（含直觉类比 + 常见误解）
├── src/                     # 源代码
├── tests/                   # 测试
├── benchmarks/
│   ├── bench_main.cpp       # benchmark 入口
│   └── results.md           # Phase benchmark 记录
├── data/                    # 测试数据
└── CMakeLists.txt
```

**开发阶段**: Phase 1(基础设施) → Phase 2(核心算法) → Phase 3(序列化) → Phase 4(工程优化) → Phase 5(评测)

**设计原则:**
- **How-Why-Simplify**: 每个模块搞清楚怎么做、为什么、mini 版如何简化
- **Phase 到基线**: 每 Phase 结束必须 benchmark 并记录
- **Task PRD 先行**: 每个 task 先写 PRD 再写代码，重点在"为什么"
- **验收驱动**: 每个 task 有明确的验收标准

详细模板见 `references/template_mini.md`

## 模式 C: 源码笔记（notes）

**输出结构:**

```
{output_dir}/
├── README.md          # 主题概述 + 文件导航 + 源码关键路径
├── distance.md        # 距离计算 + SIMD 加速
├── index_structure.md # 索引数据结构 + 内存布局
├── search_engine.md   # 查询引擎 + 搜索策略
└── ...
```

**核心结构**: 概述 → 架构(ASCII art) → 核心实现(代码+行号) → 端到端追踪 → 对比(表格) → 小结

向量库笔记推荐按维度组织: 距离计算 / 索引结构 / 查询引擎 / 构建流程 / 序列化 / 工程优化

详细模板见 `references/template_notes.md`

## 模式 D: 水平诊断（diagnose）

5 题快速诊断用户的向量检索知识水平（L0-L3），结果联动 course 模式:
- L0(零基础) → 从第 1 课开始
- L1(基础) → 跳直觉课，从数据结构课开始
- L2(中级) → 跳前 3 课，从核心算法深度课开始
- L3(高级) → 只生成进阶/对比/工程优化课时

详细模板见 `references/template_diagnose.md`

## 模式 E: 教学互动（quiz / explain）

两个子模式:

**quiz（主动回忆）**: 学完一课后 5-10 题苏格拉底式提问。不直接给答案，用引导问题帮用户自己发现。题型: 概念辨析、参数推理、设计决策、代码补全。

**explain（费曼讲解）**: 用户向 AI 解释概念，AI 扮小白追问 3-5 轮。最后给评估: 清晰度/准确性/深度 + 知识盲点 + 复习建议。

详细模板见 `references/template_pedagogy.md`

## 领域知识

算法族谱、学习路径、研究文献、Benchmark 方法论、常用开源库、常见误区等领域知识集中管理在:

→ **`references/domain_knowledge.md`**

## 内容风格

1. **标题编号**: 使用 "N.1", "N.2" 格式（N = 课时/章节号）
2. **语言**: 中文说明 + 英文代码/术语
3. **公式**: LaTeX 格式，每个公式前一句话说明直觉
4. **源码**: 关键逻辑用简化伪代码 + 源码位置标注，不大段 copy
5. **图表**: ASCII art 架构图、表格对比、内存布局字节图

## 源码阅读规范

- 每个概念标注源码位置（文件:行号）
- 代码引用使用实际函数/结构体/方法名
- 代码引用必须解释，不能只粘 Read/Grep 的结果
- recall 数据引用标注来源（ANN-Benchmarks / 自测 / 论文公开数据）
- 向量领域额外要求:
  - 距离计算必须讲 SIMD 实现（最热路径）
  - 量化公式必须配直觉（先一句话说清"在干什么"）
  - 内存布局要画字节偏移图
  - 参数影响要有 recall-QPS 曲线说明

## 质量检查清单

### 通用检查（10 项，移植自 learn-project）

- [ ] 每个概念标注了源码位置（文件:行号）
- [ ] 复杂概念先给直觉/类比，再给技术细节
- [ ] 无大段 copy 源码，只提取关键逻辑并加注释
- [ ] 对比类内容使用表格
- [ ] 每个文件末尾有小结
- [ ] README 有清晰的阅读顺序建议
- [ ] 至少有一处端到端的数据流追踪
- [ ] **代码片段必须基于真实源码验证** — 用 Read/Grep 确认行号、函数签名、成员变量名，禁止凭印象编写
- [ ] **每个设计决策都要回答"为什么"** — 不能只列"是什么"，要解释 trade-off 和意图
- [ ] **数值计算必须精确** — 字节数、参数范围等写出完整公式，用源码中的常量验证

### 向量领域检查（4 项）

- [ ] 距离计算讲了 SIMD 实现（SSE/AVX/NEON 至少提一种）
- [ ] 量化相关公式都配了直觉说明
- [ ] 核心数据结构画了内存布局字节图
- [ ] 参数影响（M/ef/nprobe 等）有 recall-QPS 关系说明

## 踩坑记录

### 坑 1: 封装层课时写空（严重）

**现象**: 封装层/胶水层课时只罗列 API，没有解释设计决策，读完什么都没学到。
**修正**: 聚焦"为什么需要这层"、关键设计模式（异常边界、锁策略）、非平凡逻辑。信息量不足则合并到相邻课时。

### 坑 2: 代码凭印象编写（严重）

**现象**: 成员变量名写错、字节数算错、函数签名凭记忆编写。
**修正**: 写课时前必须用 Read 工具读取源码确认。字节计算从源码找公式代入具体值。伪代码和真实代码明确区分。

### 坑 3: 深度不均匀

**现象**: 前半段课时深度优秀，后半段偏浅（context 腐蚀导致）。
**修正**: 使用三阶段 Context 管理策略。生成后回顾检查信息密度是否均匀。

## 导出/导入

### 导出（/learn-vec export 或 /learn-vec 导出）

将整个 skill 打包为一个 `.md` 文件，输出到**当前工作目录**。

**导出格式**: `learn-vec_export.md`
```markdown
# learn-vec skill export
<!-- version: 2.0.0 -->
<!-- exported: {timestamp} -->

## FILE: SKILL.md
（SKILL.md 完整内容）

## FILE: references/domain_knowledge.md
（完整内容）

## FILE: references/template_course.md
（完整内容）

## FILE: references/template_mini.md
（完整内容）

## FILE: references/template_notes.md
（完整内容）

## FILE: references/template_diagnose.md
（完整内容）

## FILE: references/template_pedagogy.md
（完整内容）
```

**执行步骤:**
1. 用 Glob 列出 `~/.claude/skills/learn-vec/` 下所有文件（排除图片和空目录）
2. 用 Read 读取每个文件内容，从 SKILL.md frontmatter 提取 version
3. 按上述格式拼接，用 Write 输出到 `{CWD}/learn-vec_export.md`

### 导入（/learn-vec import 或 /learn-vec 导入）

从当前目录的 `.md` 文件恢复 skill 到 `~/.claude/skills/learn-vec/`。

**执行步骤:**
1. 在当前目录查找 `learn-vec_export.md`，或让用户选择含 `<!-- version: -->` 的 .md 文件
2. 解析版本号，与当前版本比较（导入版本需 >= 当前版本，否则警告）
3. 备份当前目录（重命名为 `learn-vec.bak.{时间戳}`）
4. 解析 `## FILE:` 块，用 Write 写入对应路径
5. 验证所有文件已正确写入
