# 向量检索领域知识库

> 本文件由 learn-vec skill 引用，集中管理领域知识，避免 SKILL.md 臃肿。

## Persona 定义

你是一位向量检索领域的资深工程师和导师，擅长用直觉类比+实际源码来解释复杂概念。
你遵循"先直觉后细节"原则，每个知识点先用日常类比建立直觉，再展开技术实现。
你对常见误区保持警惕，主动帮学习者避坑。

## 算法族谱

```
向量检索算法
├── 基于空间划分
│   ├── KD-Tree, Ball Tree          — 低维有效，高维退化
│   ├── Annoy（随机投影树）          — Spotify，简单高效，适合静态数据
│   └── SPANN (2021)                — 磁盘友好的空间划分
├── 基于哈希
│   ├── LSH, SimHash, MinHash       — 理论保证，但实际精度偏低
│   └── E2LSH, Cross-Polytope LSH   — 改进版，高维更稳定
├── 基于倒排
│   ├── IVF-Flat                    — K-Means 聚类 + 倒排索引
│   ├── IVF-PQ / IVF-SQ            — 倒排 + 量化压缩
│   └── IVF 回归 (2024-2025)       — 现代 IVF+量化 在磁盘 I/O 场景可能优于 HNSW
├── 基于图
│   ├── NSW → HNSW (2018)          — 层级跳表 + 小世界图，最流行
│   ├── NSG (2019) / NSSG          — 单调搜索网络，紧凑图
│   ├── Vamana → DiskANN (2019)    — 磁盘友好图索引
│   └── HCNNG, PyNNDescent          — 近邻图构建变体
├── 量化压缩
│   ├── 标量量化（SQ）              — fp32→int8/fp16，简单高效
│   ├── 乘积量化（PQ）              — 子空间 k-means，经典压缩方案
│   ├── OPQ / LOPQ                  — 旋转优化的 PQ
│   ├── 各向异性量化（ScaNN, 2020） — Google，按重要性加权量化误差
│   └── RaBitQ (2024)              — 首个数学证明量化误差可控的方法
└── 组合方案 (2024-2025 趋势: Graph+Quantization 混合)
    ├── DiskANN + PQ                — 磁盘索引 + 内存 PQ 联合
    ├── HNSW + SQ/PQ               — 图搜索 + 量化压缩
    └── IVF + RaBitQ               — 粗筛 + 精确二进制量化
```

## 学习路径（按使用场景）

| 路径 | 场景 | 推荐模式 | 起点 |
|------|------|---------|------|
| **A** | 深入理解一个算法（如 HNSW）| course | 从直觉课开始，12 课时 |
| **B** | 从零造一个（如 mini-HNSW）| mini | Phase 1 距离计算起步 |
| **C** | 快速整理一个库（如 faiss）| notes | 按模块组织笔记 |
| **D** | 查漏补缺 | diagnose → quiz | 先诊断水平，再针对性练习 |

```
路径 A/B 推荐学习顺序:

Level 1              Level 2              Level 3              Level 4
┌──────────────┐    ┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│ 距离计算基础 │───→│ 暴力搜索+SIMD│───→│ IVF + K-Means│───→│ HNSW         │
│ L2/IP/Cos    │    │ baseline     │    │ 粗筛+倒排    │    │ 层级图搜索   │
└──────────────┘    └──────────────┘    └──────────────┘    └──────────────┘
                                              │                    │
                                              ↓                    ↓
                                        ┌──────────┐       ┌──────────────┐
                                        │ SQ 标量  │       │Vamana/DiskANN│
                                        │ 量化     │       │ 磁盘图索引   │
                                        └──────────┘       └──────────────┘
                                              │
                                              ↓
                                        ┌──────────────────────────────┐
                                        │ PQ/OPQ │ RaBitQ │ 混合索引  │
                                        │ 十亿级方案 │ recall-QPS-mem │
                                        └──────────────────────────────┘
```

## 标准源码探索协议

学习任何向量检索项目源码时，按以下 5 步进行:

1. **全局视图** — 文件树 + 构建系统 + 模块划分（用 Explore agent very thorough 扫描）
2. **入口点定位** — 找到 add/search/build 的 public API 入口函数
3. **核心数据结构** — 类成员、内存布局（画字节偏移图）、关键常量
4. **热路径追踪** — 距离计算函数、搜索主循环、构建主循环（标注 SIMD 版本）
5. **工程细节** — 序列化格式、线程安全策略、内存分配策略、prefetch 优化

每一步的发现都应记录到 `.course_log.md` 的源码路径缓存中。

## 研究文献追踪

| 论文 | 年份 | 关键 Insight |
|------|------|-------------|
| Product Quantization (Jégou et al.) | 2011 | 子空间 k-means 分解，ADC 距离计算 |
| HNSW (Malkov & Yashunin) | 2018 | 层级 NSW + Skip List 结构，高效图搜索 |
| Faiss (Johnson et al.) | 2019 | GPU 加速，工业级 IVF-PQ 实现 |
| NSG (Fu et al.) | 2019 | 单调搜索网络，紧凑图结构 |
| ScaNN (Guo et al.) | 2020 | 各向异性量化，Google 方案 |
| SPANN (Chen et al.) | 2021 | 磁盘友好空间划分，SSD 索引 |
| DiskANN (Subramanya et al.) | 2019 | Vamana 图 + PQ，磁盘索引方案 |
| RaBitQ (Gao & Long) | 2024 | 首个数学证明量化误差可控，二进制量化 |
| Annoy (Spotify) | 2013 | 随机投影树，十亿级轻量方案 |

## Benchmark 方法论

| 指标 | 说明 |
|------|------|
| **recall@K** | 与暴力搜索的 Top-K 重合度（最核心指标）|
| **QPS** | queries/sec（区分单线程/多线程）|
| **索引大小** | 每百万向量的内存/磁盘占用，注意量化压缩比 |
| **构建时间** | build_time（区分单线程/多线程）|
| **重要** | recall-QPS plot 看 Pareto 边界，单一指标无意义 |

**标准数据集**: SIFT (128d, 1M), GIST (960d, 1M), Glove (200d, 1.2M), Deep1B (96d, 1B)

**标准 Benchmark**: [ANN-Benchmarks](https://ann-benchmarks.com/)

## 常用开源库

| 库 | 语言 | 特点 |
|----|------|------|
| [hnswlib](https://github.com/nmslib/hnswlib) | C++ | HNSW 参考实现，代码精简 |
| [faiss](https://github.com/facebookresearch/faiss) | C++/Python | 工业级，IVF-PQ/HNSW/Flat，GPU 支持 |
| [DiskANN](https://github.com/microsoft/DiskANN) | C++ | 磁盘友好图索引，Vamana 算法 |
| [Annoy](https://github.com/spotify/annoy) | C++/Python | 随机投影树，Spotify 出品 |
| [ScaNN](https://github.com/google-research/google-research/tree/master/scann) | C++ | 各向异性量化，Google 出品 |
| [VSAG](https://github.com/alipay/vsag) | C++ | HNSW + FreshHNSW，蚂蚁出品 |
| [NGT](https://github.com/yahoojapan/NGT) | C++ | ANNG/ONNG，Yahoo 出品 |

## 向量领域常见误区

生成内容时主动帮学习者识别和纠正以下常见错误认知:

| 误区 | 事实 |
|------|------|
| "PQ 子空间越多越好" | 子空间越多 → 每个子空间维度越低 → 码本表达力越弱；通常 m=维度/4 到 维度/8 |
| "HNSW 的 M 越大 recall 越高" | M 过大导致图过密，搜索时遍历过多邻居反而更慢；M=12-48 通常足够 |
| "IVF nprobe 翻倍则 recall 翻倍" | recall 增长是对数型的，nprobe 翻倍收益递减 |
| "Cosine 和 L2 是完全不同的度量" | 向量归一化后，L2 排序与 Cosine 排序等价 |
| "量化只损精度不损 recall" | 量化会同时影响索引质量和搜索精度，构建时用原始向量、搜索时用量化向量是常见策略 |
| "图索引一定比倒排快" | 取决于场景：大 batch 查询、磁盘索引、极高维度下 IVF 可能更优 |
