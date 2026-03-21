# LALB 算法讲解

## 1. 核心思想：一句话版

> **以吞吐除以延时作为分流权值**，延时低的节点自然获得更多流量。

这不是简单的"按延时比例分流"。假设节点 A 延时 1ms，节点 B 延时 3ms，
LALB **不会**按 3:1 分流。而是：A 持续获得流量直到它的延时升高到和 B 持平，
或者没有更多流量了。分流比例是高度非线性的。

## 2. 直觉：为什么 QPS/Latency 能收敛

设两个节点权值 W1, W2，吞吐 QPS1, QPS2，延时 L1, L2。

权值公式：`W = QPS * SCALE / Latency^p` （p 默认为 2）

稳态分析：
- 稳态时 QPS 和 W 成正比：`W1/W2 ≈ QPS1/QPS2`
- 代入公式：`W1/W2 = (QPS1/QPS2) * (L2/L1)^p`
- 两式联立 → `L1 ≈ L2`

**结论**：算法驱动所有节点的延时趋同。延时低的节点持续获得更多流量，
直到其延时被拉升到和其他节点一样。

## 3. 四大核心机制

### 3.1 DoublyBufferedData（双缓冲读写分离）

**问题**：server 列表是读远多于写的结构，分流时不能加锁。

**方案**：
```
前台（fg）          后台（bg）
┌──────────┐       ┌──────────┐
│ server[] │       │ server[] │
│ 只读访问  │       │ 写线程改  │
└──────────┘       └──────────┘

写流程：
1. 修改后台数据
2. 切换前后台指针（原子操作）
3. 逐个获取所有 thread-local 锁并立即释放
   → 确保所有读线程都已看到新前台
4. 修改新后台（老前台）
```

**关键设计**：
- 读拿 thread-local 锁（几乎无竞争，<25ns）
- 写需要获取所有 thread-local 锁（确保读线程都切换了）
- 不同读线程之间零竞争

### 3.2 WeightTree（完全二叉树按权值查找）

**问题**：O(N) 遍历在 N=数百时因 cache miss 耗时百微秒，不可接受。

**方案**：用完全二叉树，每个节点记录左子树权值之和。

```
                    [0] left=700
                   /            \
          [1] left=400      [2] left=150
          /        \        /        \
      [3] w=400  [4] w=200  [5] w=150  [6] w=100

查找 dice=550:
  [0]: left=700, 550 < 700 → 走左子树
  [1]: left=400, 550 ≥ 400 → dice -= 400+w[3]?
       self=200, 550 ≥ 400+200=600? No → 命中 [1]?
       550 ∈ [400, 600) → 不对，这是 [4] 的范围

实际逻辑：
  index=0, dice=550
    left[0]=700, dice < 700 → index = 2*0+1 = 1
  index=1, dice=550
    left[1]=400, dice ≥ 400
    self = w[1].weight = 200 (假设)
    dice ≥ 400+200=600? 550 < 600 → 命中节点 1
```

时间复杂度 O(logN)，N=1024 时最多 10 次内存跳转，<1μs。

**和 DoublyBufferedData 的结合**：
- 前后台共享 Weight 指针（权值数据）
- `left_weight`（左子树权值和）也是共享的，用原子操作更新
- 不追求强一致性，只要最终一致

### 3.3 base_weight（基础权值计算）

```
base_weight = scaled_qps / avg_latency

其中:
  scaled_qps = (n-1) * 1000000 * WEIGHT_SCALE / time_window
  avg_latency = (bottom.latency_sum - top.latency_sum) / (n-1)
```

- 用大小为 128 的循环队列统计 QPS 和延时
- `WEIGHT_SCALE` 是放大系数，让整数存储有足够精度
- 所有权值有最小值保护（`min_weight=1000`），确保慢节点也能被探测到
- p=2 时公式为 `QPS * SCALE / latency²`，收敛更快

### 3.4 inflight delay（在途请求惩罚）

**问题**：如果只看已完成请求，当节点挂了（请求不回来），
要等到超时才能发现，期间会浪费大量请求。

**方案**：追踪在途请求的平均耗时。

```
选择 server 时：
  begin_time_sum += now
  begin_time_count++

反馈时：
  begin_time_sum -= begin_time
  begin_time_count--

inflight_delay = now - begin_time_sum / begin_time_count
```

当 `inflight_delay > avg_latency * punish_ratio` 时，
线性惩罚权值：

```
weight = base_weight * avg_latency * punish_ratio / inflight_delay
```

这能在远早于超时之前发现节点变慢，快速降低其权值。

## 4. 完整数据流

```
                        SelectServer()
                             │
         ┌───────────────────┼───────────────────┐
         ▼                   ▼                   ▼
   ┌──────────┐       ┌──────────┐       ┌──────────┐
   │  Node 0  │       │  Node 1  │       │  Node 2  │
   │  w=5000  │       │  w=3000  │       │  w=1000  │
   └──────────┘       └──────────┘       └──────────┘
         │                   │                   │
         ▼                   ▼                   ▼
     发送 RPC            发送 RPC            发送 RPC
         │                   │                   │
         ▼                   ▼                   ▼
     Feedback()          Feedback()          Feedback()
         │                   │                   │
         ├── 更新延时统计 ───┤── 更新延时统计 ───┤
         ├── 重算 base_weight                    │
         ├── 算 inflight delay                   │
         ├── 重算最终 weight                      │
         └── 更新 weight tree 父节点 ────────────┘

SelectServer 详细流程：
1. 读 DoublyBufferedData 获取 Servers 快照（无锁）
2. dice = random(0, total_weight)
3. 在完全二叉树中 O(logN) 定位节点
4. AddInflight: 重算权值，检查是否还够 dice
   - 是 → 记录 begin_time，返回该节点
   - 否 → 重新 roll dice，从根开始
5. RPC 完成后 Feedback() 更新权值
```

## 5. 关键设计取舍

| 决策 | 选择 | 原因 |
|------|------|------|
| 权值存储 | 整数 + WEIGHT_SCALE | 避免浮点运算，原子操作友好 |
| 统计窗口 | 128 个采样点 | inflight delay 能及时纠偏，128 已够 |
| 一致性 | 最终一致 | 允许短暂的权值不一致换取更高并发 |
| 最小权值 | 1000 | 慢节点也需要被探测，否则永远不知道它是否恢复 |
| 错误处理 | 混合惩罚延时 | 错误次数越多越接近 timeout，但不直接用 timeout |
| p 值 | 默认 2 | 对延时差异更敏感，收敛更快 |

## 6. 和 brpc 原实现的简化

本项目相比 brpc 原实现的简化：
1. 去掉 brpc 框架依赖（Socket、ServerId 等）
2. 用 `std::shared_mutex` 替代自定义 DoublyBufferedData（简化版）
3. 用 `std::atomic` 替代 `butil::atomic`
4. 去掉 MarkOld/ClearOld（节点移动时的前后台同步，简化版用锁保护）
5. 保留核心算法：权值计算、二叉树查找、inflight delay 惩罚
