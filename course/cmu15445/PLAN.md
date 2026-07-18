# CMU 15-445/645 (Fall 2025) 自学计划

> 目标：像真学生一样上课 —— 看课 → 做书面作业 → 写编程项目 → 提交自动评测 → 考试
> 本文档是**在另一台机器上从零搭建的执行清单**。所有结论已核实（2026-07-13），
> 不是凭记忆写的。

## 0. 先看结论：什么能还原，什么不能

| 部分 | 能否还原 | 说明 |
|------|---------|------|
| 6 次书面作业 | ✅ **原题 + 官方答案全公开** | 课程网站直接下载 |
| 5 个编程项目 | ✅ **原样还原** | BusTub 开源，有代码骨架和本地测试 |
| **自动评测机** | ✅ **官方对非 CMU 学生开放** | Gradescope 公开课程，见第 4 节 |
| 期中/期末**考纲** | ✅ 公开 | 逐章列范围，官方说"考题会像教材习题和作业题" |
| 期中/期末**真题** | ❌ **CMU_ONLY（校园网/VPN）** | 拿不到。**不要接受任何"还原的真题"——那是编的** |
| 考试替代方案 | ⚠️ 仿真卷 | 基于公开考纲 + 课件 + 教材出题，会明确标注"非真题" |

**唯一的交换条件**：官方要求 **不要把你的项目实现公开到 GitHub 或其他代码仓库**。
→ 所以代码放本地独立仓库，**不要提交到 index-pedia**（它会 push 到公司仓库）。

## 1. 关键链接

| 用途 | 地址 |
|------|------|
| 课程主页 | <https://15445.courses.cs.cmu.edu/fall2025/> |
| 作业总览（含答案） | <https://15445.courses.cs.cmu.edu/fall2025/assignments.html> |
| 课表 + 课件 + 教材章节 | <https://15445.courses.cs.cmu.edu/fall2025/schedule.html> |
| FAQ（非 CMU 学生怎么交作业） | <https://15445.courses.cs.cmu.edu/fall2025/faq.html> |
| 期中考纲 | <https://15445.courses.cs.cmu.edu/fall2025/midterm-guide.html> |
| BusTub 源码 | <https://github.com/cmu-db/bustub> |
| 课程视频 | CMU-DB 官方 YouTube（你手上是中配版） |

**教材**：Database System Concepts, 7th Edition（Silberschatz）。考纲直接按这本书的章节列范围。

## 2. 环境搭建 TODO（在新机器上执行）

**关键事实**：BusTub 官方说 **评分环境跑的是 Ubuntu 24.04**，
且明确警告 macOS 和 Linux 的测试结果可能不同。
→ **用 Ubuntu 24.04 容器**，和 Gradescope 对齐。（已决定）

- [ ] **拉代码，切到 Fall 2025 的 release tag**（重要：master 已经在往 2026 spring 漂）

  ```bash
  git clone https://github.com/cmu-db/bustub.git ~/project/github/bustub
  cd ~/project/github/bustub
  git checkout -b f2025 v20251119-2025fall
  ```

  > 这个 tag 是官方打的 Fall 2025 发布点，对应你要看的这一届课。

- [ ] **写一个 Ubuntu 24.04 的 Dockerfile**（仓库里没有，要自己写）
  - 基础镜像 `ubuntu:24.04`
  - 装 `build_support/packages.sh -y` 需要的东西（脚本只认 22.04 / 24.04，认别的直接 give_up）
  - 工具链：**clang-15**（`packages.sh` 里写死 `CLANG_VERSION=15`）、cmake、gcc、python3
  - 容器名建议 `bustub-dev-arm64`，和你 VSAG 的 `vsag-dev-arm64` 一个风格
  - 把 `~/project/github/bustub` 挂进去，别把源码打进镜像

- [ ] **验证编译 + 跑通测试**

  ```bash
  mkdir build && cd build
  cmake -DCMAKE_BUILD_TYPE=Debug ..
  make -j$(nproc)
  make check-tests          # 本地 gtest
  ```

- [ ] **确认 sanitizer 可用**（后面 P4 并发一定用得上）

  ```bash
  cmake -DCMAKE_BUILD_TYPE=Debug -DBUSTUB_SANITIZER=thread ..
  ```

- [ ] **签自动评测的协议**（提交前必须做一次）

  ```bash
  python3 gradescope_sign.py
  ```

## 3. 五个编程项目（Fall 2025 实际内容）

> 以下不是从记忆里写的，是从 `v20251119-2025fall` 这个 tag 的源码里扒出来的
> （`grep -rl "TODO(P" src/`）。**注意 Fall 2025 的 P0 和网上流传的老版本不一样。**

| 项目 | 你要实现的东西 | 关键文件 |
|------|---------------|---------|
| **P0** C++ Primer | **跳表（Skip List）** ← 不是老版本的 Trie | `src/primer/skiplist.cpp` |
| **P1** Buffer Pool Manager | 磁盘调度器、**LRU-K 替换器**、**ARC 替换器**、页守卫（page guard）、缓冲池 | `src/buffer/`、`src/storage/disk/disk_scheduler.cpp`、`page_guard.cpp` |
| **P2** Database Index | **B+ 树**（插入/删除/迭代器/并发控制） | `src/storage/index/b_plus_tree.cpp`、`b_plus_tree_{leaf,internal}_page.cpp` |
| **P3** Query Execution | 各种算子（聚合、连接、**外部归并排序**）、优化器规则（顺序扫描改索引扫描） | `src/execution/executors/`、`src/optimizer/seqscan_as_indexscan.cpp` |
| **P4** Concurrency Control | **MVCC**（undo log、事务、水位线、垃圾回收） | `src/concurrency/`、`src/execution/execution_common.h` |

**对你的难度预判**（8 年 C++ 后台 + 存储引擎背景）：
- P0 跳表：热身，半天
- P1 缓冲池：本行，LRU-K/ARC 你闭着眼写；重点是 page guard 的 RAII 和引用计数
- P2 B+ 树：**最耗时的一个**，并发那部分（螃蟹锁 crabbing）是真难点
- P3 查询执行：概念新（火山模型），但代码不难
- P4 MVCC：概念密度最高，是整门课的收官

## 4. 自动评测怎么用（这是最关键的发现）

官方 FAQ 原文：**"We will make the auto-grader for each assignment available to
non-CMU students on Gradescope after their due date for CMU students."**

- [ ] 注册 Gradescope，加入公开课程
  - **Entry Code: `5R4XPZ`**
  - School: **Carnegie Mellon University**
- [ ] 每个项目按官方 spec 打包提交（每个 project 页面有 `zip` 打包命令）
- [ ] 本地 `make check-tests` 只覆盖一部分，**隐藏测试在 Gradescope 上**

> 这一条解决了你原来的痛点。你之前以为"自学没有练习题和环境"，
> 实际上官方把评测机对外开放了，只是藏在 FAQ 里。

## 5. 十四周课表（官方 Fall 2025 时间线）

课件和录像按 Lecture 号对应，教材章节直接抄自官方 schedule。

| 周 | Lecture | 主题 | 教材章节 | 该做什么 |
|----|---------|------|---------|---------|
| 1 | 1-2 | 关系模型与代数、现代 SQL | Ch 1-2, 3-5 | 起 **P0 跳表**、**HW1 SQL** |
| 2 | 3-4 | 数据库存储 I、内存管理 | Ch 12.1-12.4, 13.2-13.5 | 起 **P1 缓冲池** |
| 3 | 5-6 | 数据库存储 II、存储模型与压缩 | Ch 14.8.1, 24.2, 11.2, 13.6 | **HW2 存储** |
| 4 | 7-8 | 哈希表、索引与过滤器 I | Ch 14.5, 24.5, 14.1-14.4 | 交 P1 |
| 5 | 9-10 | 索引与过滤器 II、索引并发控制 | Ch 14.1-14.4, 24.1, 18.10.2 | **HW3 索引**、起 **P2 B+树** |
| 6 | 11-12 | 排序与聚合、连接算法 | Ch 15.4-15.6 | 继续 P2 |
| 7 | — | **期中考试**（80 分钟，覆盖 Lecture 1-11） | — | 见第 6 节 |
| 8 | 13-14 | 查询执行 I / II | Ch 15.1-15.3, 15.7, 22 | 交 P2、起 **P3 查询执行** |
| 9 | 15-16 | 查询规划与优化 I / II | Ch 16 | **HW4 执行与规划** |
| 10 | 17-18 | 并发控制理论、两阶段锁 | Ch 18, 18.1-18.3, 18.9 | 交 P3 |
| 11 | 19-20 | 时间戳排序、多版本并发控制 | Ch 18.5-18.8 | **HW5 事务**、起 **P4 MVCC** |
| 12 | 21-22 | 数据库日志、数据库恢复 | Ch 19.1-19.9 | 继续 P4 |
| 13 | 23-24 | 分布式系统 I / II | Ch 20-23 | **HW6 恢复** |
| 14 | 25 | 期末复习与系统 | — | 交 P4、期末 |

## 6. 两台机器的分工（实际学习节奏）

```
   公司电脑（输入）                       家里个人电脑（输出）
   ─────────────────                      ──────────────────
   看 Lecture 录像（中配版）              写 BusTub 项目代码
   读教材对应章节                         做书面作业（HW1-6，官方原题）
   读论文                                 做笔试卷（我出的仿真题）
   记笔记 → notes/                        提交 Gradescope 拿分
        |                                        ^
        |                                        |
        +--------- git push/pull ----------------+
                index-pedia 当传送带
```

**传送带约定**：
- 我出的卷子、你的笔记、作业解答，全在 `index-pedia/dev_fengbai/course/cmu15445/` 下，
  git push 后回家 pull 就有
- **只有 BusTub 的代码不进这个仓库**（官方要求不公开实现，且这仓库要 push 到公司）

## 7. 笔试卷：三档，跟着课走

**真题拿不到**（`files/CMU_ONLY/` 要 CMU 校园网或 VPN）。所以是我出题，
**每份卷子标题都会写清"仿真题，非 CMU 真题"**，绝不冒充。

出题依据（不是凭记忆编）：官方**公开考纲** + 该节课的**官方课件 PDF** +
教材指定章节。官方考纲原话：考题风格接近**教材习题和书面作业题**。

| 档位 | 形式 | 时长 | 什么时候出 |
|------|------|------|-----------|
| **随堂小测** | 8-12 道选择/判断 + 2 道简答 | 20 分钟 | **每看完 1-2 节课，你说一声我就出** |
| **期中仿真卷** | 选择 + 简答 + 计算/画图 | 80 分钟（对齐官方） | 看完 Lecture 1-11 |
| **期末仿真卷** | 同上，覆盖全课 | 3 小时 | 看完 Lecture 25 |

每份卷子交付两个文件：
- `exam/LNN_quiz.md` —— 题目（**不含答案**，直接做）
- `exam/LNN_quiz_solution.md` —— 答案 + 解析 + 每题标注考点出处（对应哪页课件/哪章）

**高性价比补充**：官方既然说考题像教材习题，那把《Database System Concepts》7th
对应章节的课后习题刷一遍，比任何"还原真题"都靠谱。我出的小测会刻意往这个风格靠。

## 8. 目录约定（重要：别把代码 push 上去）

```
   ~/project/github/bustub/           ← 代码。独立仓库，【不要】提交到 index-pedia
                                         官方明确要求不要公开你的项目实现

   index-pedia/dev_fengbai/course/cmu15445/
       PLAN.md                        ← 本文档
       progress.md                    ← 进度打卡（看到哪、做到哪）
       notes/                         ← 每节课的笔记（公司电脑写）
       homework/                      ← 书面作业解答（家里写）
       exam/                          ← 我出的仿真卷 + 答案（家里做）
```

## 9. 待办清单（回家后按顺序做）

**第一次搭环境（一次性）**

- [ ] 1. clone BusTub，切到 `v20251119-2025fall`
- [ ] 2. 写 Ubuntu 24.04 Dockerfile，起容器 `bustub-dev-arm64`
- [ ] 3. 编译 + `make check-tests` 全绿
- [ ] 4. 注册 Gradescope（Entry Code `5R4XPZ`），跑 `gradescope_sign.py`
- [ ] 5. 下载课程材料：`bash fetch_materials.sh`
      （课件 + 课后标注笔记 + 书面作业，输出到 `~/project/cmu15445-fall2025/`，
      **答案单独放 `solutions/`，做完再看**）

**然后进入循环**

- [ ] 公司：看 Lecture N → 记笔记
- [ ] 回家：跟我说"看完 Lecture N 了" → 我出小测 → 你做 → 对答案
- [ ] 回家：推进对应的 BusTub 项目 → 本地测试 → 交 Gradescope
- [ ] 从 Lecture 1-2 + P0 跳表 + HW1 开始

## 9. 15-721（高级数据库）—— 暂缓

已核实：这门课**根本不存在"习题"**，结构完全不同：

- 15% 论文阅读 review（每节课前交一段话）
- 10% 当一次课的 LaTeX 记录员
- **60% 三人小组项目**：基于 Apache DataFusion + optd，做云原生 OLAP 的一个组件
  （调度器 / 执行引擎 / catalog / I/O 与分布式缓存 / 优化器），**开放式，无自动评测、无标准答案**
- 15% 期末 take-home 长问答（题目不公开）

→ 决定：**先专注 15-445**。等基础打完，15-721 按"论文精读营 + 一个结合你方向
（向量检索 / 存储引擎）的自选项目"来上，比硬套官方项目更有价值。
