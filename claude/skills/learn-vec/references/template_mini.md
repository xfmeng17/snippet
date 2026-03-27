# 模式 B 模板: 手写 Mini 版

## TASK.md 模板

```markdown
# {MiniProjectName} — {一句话定位}

> 例: NanoHNSW — 从零手写 HNSW 理解图索引的核心设计

## 目标
通过从零手写 {算法/引擎} 的核心流程，深入理解向量检索中的 {主题}。
搞清楚三件事:
1. **How** — 索引构建和查询搜索是怎么设计，怎么实现的
2. **Why** — 为什么这样设计？解决了什么问题？有哪些取舍？
3. **Simplify** — Mini 版做了哪些简化？为什么可以简化？

## 参考
- 原始论文: {论文名，链接}
- 参考实现: {开源库路径，如 ~/project/hnswlib}
- 关键模块速查见 `docs/architecture.md`

## 技术约束
- 语言: C++17
- 构建: CMake 3.16+
- 测试: Google Test
- 代码风格: Google C++ Style（禁止异常，限制 auto，强制大括号）
- 距离度量: 默认 L2，可扩展 IP/Cosine

## 核心流程

Build:                          Search:
vectors[] ──→ 逐个插入          query ──→ 从顶层开始
     │                              │
     ├─ 选择插入层级                ├─ 逐层贪心搜索
     │                              │
     ├─ 在每层搜索最近邻            ├─ 到达 Level 0
     │                              │
     ├─ 双向连边 + 剪枝            ├─ beam search
     │                              │
     └─ 更新邻居表                  └─ 返回 Top-K

## 验收基线
- recall@10 >= 0.95 (SIFT1K, ef=200)
- 构建 10K 128d 向量 < 5s
- 支持 save/load
- 全部测试通过

## 子任务拆解

### Phase 1: 基础设施
| # | 子任务 | 说明 | 状态 |
|----|--------|------|------|
| 01 | 距离计算 | L2/IP distance，暴力搜索 baseline | TODO |
| 02 | 向量存储 | 内存管理，对齐分配 | TODO |

### Phase 2: 核心算法
| # | 子任务 | 说明 | 状态 |
|----|--------|------|------|
| 03 | {核心算法 A} | ... | TODO |
| 04 | {核心算法 B} | ... | TODO |

### Phase 3: 序列化
| # | 子任务 | 说明 | 状态 |
|----|--------|------|------|
| 05 | 索引持久化 | save/load 到文件 | TODO |

### Phase 4: 工程优化
| # | 子任务 | 说明 | 状态 |
|----|--------|------|------|
| 06 | SIMD 加速 | SSE/AVX 距离计算 | TODO |
| 07 | 多线程 | 并行构建/搜索 | TODO |

### Phase 5: 评测
| # | 子任务 | 说明 | 状态 |
|----|--------|------|------|
| 08 | Benchmark | recall@K, QPS, 内存占用 | TODO |
| 09 | 对比测试 | 与 {原库} 在相同数据集上对比 | TODO |
```

## Phase Benchmark 约束

**每个 Phase 结束后必须 benchmark 并记录**，不允许跳过:

| Phase | Benchmark 内容 | 最低要求 |
|-------|---------------|---------|
| Phase 1 | 暴力搜索 recall@10（验证距离计算正确性）| recall@10 = 1.00 |
| Phase 2 | 核心算法 recall@10 + QPS vs 暴力搜索 | recall@10 >= 0.90 |
| Phase 3 | save → load → search 结果一致性 | recall 无损 |
| Phase 4 | SIMD 加速比 + 多线程加速比 | SIMD >= 2x, 多线程近线性 |
| Phase 5 | 完整 benchmark 报告 | 达到验收基线 |

Benchmark 记录格式（写入 `benchmarks/results.md`）:
```markdown
## Phase {N} Benchmark — {日期}
| 指标 | 值 | 说明 |
|------|-----|------|
| recall@10 | 0.XX | SIFT1K, ef=200 |
| QPS | XXXX | 单线程, 128d, 10K vectors |
| build_time | X.Xs | 10K vectors |
| memory | XX MB | 10K 128d vectors |
```

## Task PRD 模板 (tasks/taskNN_xxx.md)

```markdown
# Task NN: {任务名}

## 目标
要构建什么（一句话）

## 为什么（Why）
- 在原算法/引擎中，这个模块解决什么问题
- 为什么这样设计（论文/原实现的设计理念）
- Mini 版做了哪些简化，为什么可以简化

## 需求

具体要实现什么:
- 文件: `{project}/include/{module}/{file}.h`
- 文件: `{project}/src/{module}/{file}.cpp`
- 类/函数: ...
- 接口: ...

## 设计要求
- 架构约束
- 与原算法/实现的对应关系
- 内存布局要求（如对齐）

## 性能要求（如有）
- 距离计算: {目标 GFLOPS 或与暴力搜索的加速比}
- 内存: {每个向量的额外开销上限}

## 验收标准
- [ ] 编译通过
- [ ] 单测通过: tests/{module}/xxx_test.cpp
- [ ] recall 验证: recall@10 >= {目标} on {数据集}
- [ ] 性能验证: {QPS 或构建速度目标}

## 参考
- 论文: {Section X.Y, 算法 N}
- 参考实现: `{源码路径}:{行号范围}`
- 相关文档: `docs/{topic}.md`
```

## architecture.md 模板

```markdown
# {MiniProject} 架构 — 与 {原算法/引擎} 的对照

## 模块对照
| Mini 版模块 | 原实现对应 | 简化说明 |
|------------|-----------|---------|
| distance.h | {原路径} | 只实现 L2/IP，去掉 SIMD (Phase 1)，Phase 4 补 SIMD |
| storage.h | {原路径} | 简化为 std::vector，去掉 mmap |
| ... | ... | ... |

## 算法对照
| 步骤 | 论文描述 | 原实现 | Mini 版 |
|------|---------|--------|---------|
| 邻居选择 | Algorithm 3 | {函数名} | 简化版 heuristic |
| ... | ... | ... | ... |

## 数据流
用 ASCII art 画出 Mini 版的数据流:

Input vectors
     │
     ↓
┌──────────┐    ┌──────────┐    ┌──────────┐
│ Distance │───→│  Build   │───→│  Index   │
│ Compute  │    │ (insert) │    │ (graph)  │
└──────────┘    └──────────┘    └──────────┘
                                     │
                                     ↓
                                ┌──────────┐    ┌──────────┐
                                │  Search  │───→│ Results  │
                                │ (query)  │    │ (top-K)  │
                                └──────────┘    └──────────┘

## 目录结构

{project}/
├── include/
│   ├── distance.h          # 距离计算
│   ├── storage.h           # 向量存储
│   └── index.h             # 索引接口
├── src/
│   ├── distance.cpp
│   ├── storage.cpp
│   └── index.cpp
├── tests/
│   ├── distance_test.cpp
│   └── index_test.cpp
├── benchmarks/
│   ├── bench_main.cpp
│   └── results.md          # Phase benchmark 记录
├── data/                   # 测试向量
└── CMakeLists.txt
```

## algorithm_notes.md 模板

```markdown
# {算法名} 核心笔记

> 配合 Task PRD 使用，记录算法理解过程中的关键 insight

## 直觉类比
用一个日常类比概括算法的核心思想。
例: "HNSW 就像一座城市的交通网络——高速公路（上层）连接城市，市区道路（底层）连接每栋楼"

## 核心 Insight
一句话概括算法的本质思想

## 关键公式

### {公式名}
**直觉**: {一句话说清这个公式在做什么}

$$
{LaTeX 公式}
$$

**推导要点**: {简要推导，不要纯堆步骤}

## 复杂度分析
| 操作 | 时间复杂度 | 空间复杂度 | 说明 |
|------|-----------|-----------|------|
| 构建 | O(...) | O(...) | ... |
| 查询 | O(...) | O(...) | ... |

## 常见误解

| 误解 | 事实 |
|------|------|
| {常见错误认知 1} | {正确理解} |
| {常见错误认知 2} | {正确理解} |

## 与其他算法的关键差异
| 维度 | {本算法} | {对比算法 A} | {对比算法 B} |
|------|---------|------------|------------|
| 索引结构 | ... | ... | ... |
| 查询策略 | ... | ... | ... |
| 内存占用 | ... | ... | ... |
| 适用场景 | ... | ... | ... |
```

## .claude/CLAUDE.md 模板

```markdown
# {MiniProject} 开发规则

## 核心原则
- 每个模块必须写清楚 "Why"（为什么原算法这样设计）
- 先写测试，再写实现
- 完成一个 task 就提交一次
- 距离计算是最热路径，优化时优先关注

## 代码风格
- Google C++ Style
- 禁止 try-catch / throw，用返回值报告错误
- 限制 auto，写明确类型名
- for/if/while 必须 `{}`

## 测试要求
- 每个函数对应单测
- 距离计算用暴力搜索结果做 ground truth
- recall 测试用标准数据集（SIFT1K 起步）

## Benchmark 要求
- 每个 Phase 结束后跑 benchmark 并记录到 benchmarks/results.md
- 记录: recall@K, QPS, build_time, memory
- 与原库在相同数据集上对比

## 提交规范
- feat/fix/docs/bench + task 编号
- 例: `feat(task03): implement HNSW insert with layer selection`
```
