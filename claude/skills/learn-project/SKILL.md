---
name: learn-project
version: "1.1.0"
description: 学习开源项目/代码库的通用技能。支持三种模式：源码笔记(notes)、分课时讲解课程(course)、从零手写 mini 版(mini)。当用户说"学习 XX 项目"、"整理 XX 源码"、"learn-project"时触发。支持 "/learn-project export/导出" 和 "/learn-project import/导入"。
---

# Learn Project - 开源项目深度学习技能

> **Version**: 1.1.0

通用的项目/代码库学习技能，帮助用户系统性地理解一个开源项目的设计与实现。
不只是"浏览代码"，而是让你真正学会一个算法/架构的 How、Why、Trade-off。

## 触发条件

- `/learn-project [mode] [source_path] [output_path]`
  - 例: `/learn-project course ~/project/vsag ./hnsw_learn`
  - 例: `/learn-project mini ~/project/ant_are ./nanoha3`
  - 例: `/learn-project notes ~/project/vsag`
- 自然语言: "学习 XX 项目"、"帮我整理 XX 源码"、"教我 XX 的实现"

**参数说明**（均可省略，省略时交互确认）:
- `$0` / mode: `notes` | `course` | `mini`
- `$1` / source_path: 要学习的代码库路径
- `$2` / output_path: 输出目录（可选）

## 三种学习模式

| 模式 | 适用场景 | 产出 | 典型输入 |
|------|---------|------|---------|
| **A: notes** | 整理一个模块/组件的源码 | README + 分主题 md | "整理 VSAG 的稀疏索引" |
| **B: course** | 从零学一个算法/模块，需递进讲解 | README(大纲) + 分课时 md | "教我 HNSW，我没基础" |
| **C: mini** | 通过手写简化版来理解原项目 | TASK.md + tasks/ + docs/ + 代码 | "手写 mini 搜索引擎学 HA3" |

## 工作流程

```
用户输入（自然语言 或 /learn-project [args]）
  │
  ├─ 1. 确认参数
  │     解析 $ARGUMENTS 或交互确认：模式、源码路径、输出目录
  │     如果用户没指定模式，根据输入推断后用 AskUserQuestion 确认
  │
  ├─ 2. 项目原型检测
  │     自动识别项目类型，匹配探索策略（见下方）
  │
  ├─ 3. 深度探索源码
  │     用 Explore agent 深度分析（very thorough）
  │     a. 全面扫描: 文件树、构建系统、入口点
  │     b. 核心识别: 5-10 个最重要的文件
  │     c. 数据流追踪: 输入 → 处理 → 输出的完整路径
  │     d. 算法定位: 关键算法的入口函数和调用链
  │     e. 对比发现: 如果有多个实现，分析设计取舍
  │
  ├─ 4. 规划内容结构
  │     模式 A: 按模块/维度组织笔记大纲
  │     模式 B: 设计课时大纲（从直觉到细节，递进式）
  │     模式 C: 拆解 Phase → Task
  │
  ├─ 5. 生成内容
  │     并行创建所有文档/代码文件
  │
  └─ 6. 输出摘要
        文件列表 + 建议阅读顺序 + 推荐的下一步行动
```

## 项目原型检测

探索源码前，先识别项目类型以匹配探索策略：

| 原型 | 特征文件 | 探索重点 |
|------|---------|---------|
| C++ 库/引擎 | CMakeLists.txt, Makefile, .h/.cpp | 类层次、内存布局、算法复杂度 |
| Java 服务 | pom.xml, build.gradle, src/main/ | 接口定义、依赖注入、配置 |
| Python 库 | setup.py, pyproject.toml | 模块结构、API 设计 |
| 前端应用 | package.json, src/components/ | 组件树、状态管理、数据流 |
| 数据系统 | 含 index/query/storage 目录 | 存储格式、查询路径、一致性 |
| ML 系统 | model/, train/, inference/ | 模型架构、训练流程、推理优化 |

## 模式 A: 源码笔记（notes）

### 输出结构

```
{output_dir}/
├── README.md          # 主题概述 + 文件导航
├── {topic1}.md        # 子主题笔记
├── {topic2}.md
└── ...
```

### 笔记模板

见 `references/template_notes.md`。核心结构：
- 概述 → 架构(ASCII art) → 核心实现(代码片段+行号) → 对比(表格) → 小结

## 模式 B: 课程讲解（course）

### 输出结构

```
{output_dir}/
├── README.md              # 课程大纲 + 源码导航 + 阅读建议
├── .course_log.md         # 生成过程追踪日志（质量锚点 + 进度 + 风格规范）
├── 01_{first_topic}.md    # 课时 01: 建立直觉（不看代码）
├── 02_{second_topic}.md   # 课时 02: 核心数据结构
├── 03_{third_topic}.md    # 课时 03: 核心算法 A
├── ...
└── NN_{last_topic}.md     # 课时 NN: 总结对比
```

### .course_log.md —— 防止 Context 腐蚀的关键文件

**背景**：课程通常 8-12 课时，后半段生成时 context 已被前面课时的源码、探索结果、文档内容大量占据，导致质量下降（典型症状：代码凭印象编、缺少"为什么"、深度骤降）。

**机制**：在输出目录生成 `.course_log.md`，作为跨课时的"质量锚点"。每完成一课更新一次。

```markdown
# Course Generation Log

## 质量标杆
<!-- 生成前 2-3 课后，从中提取写作风格规范，后续课时对照执行 -->
- 代码风格: 真实源码片段 + 文件:行号标注，伪代码标 "(简化)"
- 解释风格: 先"为什么需要" → 再"怎么做" → 设计点标注 trade-off
- 可视化: 每节至少 1 个 ASCII 图/表格/流程图
- 信息密度参考: 课时 02 约 350 行、课时 03 约 400 行（±20%为正常范围）

## 进度追踪
| 课时 | 文件 | 状态 | 行数 | 备注 |
|------|------|------|------|------|
| 01 | 01_xxx.md | done | 280 | |
| 02 | 02_xxx.md | done | 350 | 质量标杆课 |
| ... | ... | ... | ... | ... |

## 关键源码路径缓存
<!-- 避免后续课时重复探索，也避免 context 中塞满重复的 Read 结果 -->
- HGraph 主类: src/algorithm/hgraph.h (class HGraph : public InnerIndexInterface, line 50)
- HGraph 成员: line 358-407
- ...
```

### 生成策略：Context 管理

课时按以下策略分批生成，防止 context 腐蚀：

**第 1-4 课时（context 新鲜期）**：
- 正常生成，源码探索结果直接在 context 中
- 第 2-3 课完成后，更新 `.course_log.md` 的"质量标杆"section，提取风格规范

**第 5 课时起（context 膨胀期）**：
- 每个课时用独立的 Agent 生成（`subagent_type: general-purpose`）
- Agent prompt 中包含：
  1. `.course_log.md` 的完整内容（质量标杆 + 源码路径缓存）
  2. 该课时的具体要求（标题、覆盖的源码文件、要讲的知识点）
  3. 前一课时的"本课小结"section（保持衔接）
  4. 明确指令："用 Read 工具验证所有代码片段和行号"
- Agent 完成后，主 context 只接收产出文件，不接收探索过程
- 更新 `.course_log.md` 的进度行

**最后 1-2 课时（对比/总结课）**：
- 同样用独立 Agent
- 额外传入所有课时的"本课小结"section（通常每课 5-10 行，总共不超过 100 行）

### 课程设计原则

1. **先直觉后代码**：第一课用类比和图解建立直觉，不贴源码
2. **递进式深入**：概念 → 数据结构 → 核心算法 → 工程细节 → 进阶主题
3. **每课自包含**：每课有明确的学习目标、内容、小结
4. **源码锚定**：每个概念标注对应源码位置（`文件路径:行号`）
5. **可视化优先**：大量使用 ASCII 图、表格、伪代码步骤演示
6. **端到端跟踪**：至少有一课追踪一个完整的数据路径（如一次查询从入口到结果）

### 课时模板

见 `references/template_course.md`

## 模式 C: 手写 Mini 版（mini）

### 输出结构

```
{output_dir}/
├── TASK.md                      # 总路线图
├── .claude/CLAUDE.md            # 项目级 Claude 规则
├── tasks/                       # 子任务 PRD
│   ├── task01_{name}.md
│   └── ...
├── docs/
│   └── architecture.md          # 模块对照图（mini vs 原项目）
├── {project_name}/              # 源码（后续按 task 逐个实现）
├── tests/                       # 测试
└── data/                        # 测试数据
```

### 设计原则

1. **How-Why-Simplify**：每个模块搞清楚原项目怎么做的、为什么、mini 版如何简化
2. **Phase 拆解**：按依赖关系分阶段，每阶段有可运行的里程碑
3. **Task PRD 先行**：写完 PRD 再写代码，PRD 重点在"为什么"
4. **验收驱动**：每个 task 有明确的验收标准（通常是测试）

### TASK.md / Task PRD 模板

见 `references/template_mini.md`

## 通用规范

### 内容风格

- **语言**: 中文说明 + 英文代码/术语
- **代码**: 关键逻辑用简化伪代码 + 源码位置标注，不要大段 copy 源码
- **图表**: ASCII art 架构图、表格对比、流程图
- **标题编号**: 使用 `N.1`、`N.2` 格式（N = 课时/章节号）
- **难度标注**（可选）: 在 README 大纲中标注每节的难度（基础/进阶/深入）

### 质量检查清单

- [ ] 每个概念标注了源码位置（文件:行号）
- [ ] 复杂概念先给直觉/类比，再给技术细节
- [ ] 无大段 copy 源码，只提取关键逻辑并加注释
- [ ] 对比类内容使用表格
- [ ] 每个文件末尾有小结
- [ ] README 有清晰的阅读顺序建议
- [ ] 至少有一处端到端的数据流追踪
- [ ] **代码片段必须基于真实源码验证**——用 Read/Grep 确认行号、函数签名、成员变量名，禁止凭印象编写
- [ ] **每个设计决策都要回答"为什么"**——不能只列"是什么"，要解释 trade-off 和意图
- [ ] **数值计算必须精确**——字节数、参数范围等写出完整公式，用源码中的常量验证

## 历史案例

| 项目 | 模式 | 输出目录 | 说明 |
|------|------|----------|------|
| VSAG HNSW | B (course) | `{project}/data/{user}/hnsw_learn/` | 12 课时，从算法直觉到 HGraph 对比 |
| NanoHA3 | C (mini) | `~/project/{user}/nanoha3/` | 18 task，5 phase，手写搜索引擎 |

## 注意事项

- 输出目录默认规则：
  - 模式 A/B: `{项目根目录}/data/{user_dir}/{topic}_learn/`
  - 模式 C: `~/project/{user}/{mini_project_name}/`
- 模式 C 第一步只生成框架（TASK.md + tasks/ + docs/），不写源码
- 大型项目先问用户关注哪些模块，不要试图覆盖全部
- 如果用户之前已有学习产出，先检查避免重复

## 踩坑记录（来自实际 Review 反馈）

以下是从 VSAG HNSW 课程 Review 中总结的典型问题，生成内容时务必规避：

### 坑 1：封装层/胶水层课时容易写空（严重）

**现象**：课时 08（VSAG 封装层）只是罗列了类成员和代码片段，没有解释封装层存在的意义和设计决策，读完什么都没学到。与相邻课时（07 序列化、09 HGraph 架构）的质量差距明显。

**根因**：封装层不包含"核心算法"，容易误以为"没什么可讲的"，陷入单纯罗列 API 的模式。

**修正**：封装层课时应聚焦于：
- **为什么需要这层**：用对照表展示"算法层提供的" vs "用户需要的"之间的 gap
- **关键设计模式**：异常边界（如 SAFE_CALL）、锁策略、状态管理
- **非平凡逻辑**：参数校验规则的意图、Tombstone 恢复流程、连通性检查等
- 如果真的没什么可讲，考虑合并到相邻课时而非单独成课

### 坑 2：代码片段凭印象编写，与源码不符（严重）

**现象**：成员变量名写错（`use_reorder_` 实际不存在，应为 `ignore_reorder_` + `reorder_`）；字节数算错（邻居区写 130B，实际 132B）；函数签名、参数列表凭记忆编写而非从源码复制。

**修正**：
- 写课时前必须用 Read 工具实际读取源码文件，确认成员变量名、函数签名、行号
- 涉及字节计算时，从源码中找到计算公式（如 `size_links_level0_ = maxM0_ * sizeof(InnerIdType) + sizeof(linklistsizeint)`），代入具体值，写出完整推导过程
- 伪代码和真实代码要明确区分（标注"简化"或给出真实行号）

### 坑 3：深度不均匀

**现象**：课时 01-07（hnswlib 内核）深度一致且优秀；课时 09-11（HGraph）虽然准确但部分内容偏浅（如 O-Descent 只讲了一页、ParallelSearcher 三行带过、ShrinkAndRepair 只列状态名）。

**修正**：
- 生成课时大纲后，评估每课的信息密度是否均匀
- 如果某个主题确实不需要深入，明确标注为"概览"并在 README 中说明
- 对比类课时（如 NSW vs O-Descent）双方应达到相近的深度

## 导出/导入（跨机器迁移 skill 本身）

### 导出 (`/learn-project export` 或 `/learn-project 导出`)

将整个 skill 打包为一个 `.md` 文件，输出到**当前工作目录**，方便拷贝到其他机器。

**导出格式**: `learn-project_export.md`，内容结构如下：
```markdown
# learn-project skill export
<!-- version: 1.1.0 -->
<!-- exported: {timestamp} -->

## FILE: SKILL.md
（SKILL.md 完整内容）

## FILE: references/template_notes.md
（完整内容）

## FILE: references/template_course.md
（完整内容）

## FILE: references/template_mini.md
（完整内容）
```

**执行步骤**:
1. 用 Glob 列出 `~/.claude/skills/learn-project/` 下所有文件（排除空目录）
2. 用 Read 读取每个文件内容，从 SKILL.md frontmatter 提取 `version` 字段
3. 按上述格式拼接（将版本号写入 `<!-- version: x.y.z -->` 头），用 Write 输出到 `{CWD}/learn-project_export.md`
4. 告知用户输出路径和版本号

### 导入 (`/learn-project import` 或 `/learn-project 导入`)

从当前目录的 `.md` 文件恢复 skill 到 `~/.claude/skills/learn-project/`。

**执行步骤**:
1. 在当前目录查找导入文件。优先找 `learn-project_export.md`；若不存在，列出当前目录的 `.md` 文件让用户选择
2. 用 Read 读取该 `.md` 文件
3. 解析 `<!-- version: x.y.z -->` 获取导入版本，与当前 SKILL.md frontmatter 中的 version 比较
   - 导入版本更新 → 继续导入
   - 导入版本相同 → 提示用户版本相同，确认是否覆盖
   - 导入版本更旧 → 警告用户当前版本更新，确认是否降级
4. 解析每个 `## FILE: <相对路径>` 区块，提取代码块中的文件内容
5. **备份**已有的 `~/.claude/skills/learn-project/` 目录（重命名为 `learn-project.bak.时间戳`）
6. 用 Write 将每个文件写入 `~/.claude/skills/learn-project/<相对路径>`
7. 告知用户恢复结果（包含版本号变化信息）
