# LALB 算法与核心实现讲解

## 1. 核心思想：一句话版

> **以吞吐除以延时作为分流权值**，延时低的节点自然获得更多流量。

这不是简单的"按延时比例分流"。假设节点 A 延时 1ms，节点 B 延时 3ms，
LALB **不会**按 3:1 分流。而是：A 持续获得流量直到它的延时升高到和 B 持平，
或者没有更多流量了。分流比例是高度非线性的。

## 2. 直觉：为什么 QPS/Latency 能收敛

权值公式：`W = QPS * SCALE / Latency`

稳态推导：
- 稳态时 QPS 和 W 成正比：`W1/W2 ≈ QPS1/QPS2`
- 代入公式：`W1/W2 = (QPS1/QPS2) * (L2/L1)`
- 两式联立 → `L1 ≈ L2`

**结论**：算法驱动所有节点的延时趋同。

### 数值演示

假设 2 个 server，总 QPS=1000。A 容量大（满载延时 5ms），B 容量小（满载延时 10ms）。

```
轮次    A延时    B延时    A权值        B权值        A流量占比
初始    5ms      10ms     200/ms       100/ms       67%
 10     6ms      8ms      167/ms       125/ms       57%
 50     7ms      7.5ms    143/ms       133/ms       52%
100     7.2ms    7.2ms    139/ms       139/ms       50%
```

关键观察：
- **不是**按延时反比 2:1 分流 → 那样 A 得 667 qps、B 得 333 qps
- 而是 A 持续吃流量直到它被压到和 B 一样慢
- 最终两者延时趋同（7.2ms），A 因容量大实际承担更多，但延时拉平了
- 如果 B 突然变慢（比如 GC），它的权值瞬间下降，流量立即转移

## 3. 核心并发机制

### 3.1 DoublyBufferedData（双缓冲读写分离）

**源码**：`lalb/doubly_buffered_data.h`
**brpc 原版**：`butil/containers/doubly_buffered_data.h`

#### 3.1.1 问题

server 列表是读远多于写的结构。Select 每秒被调用数万到数百万次，
而 AddServer/RemoveServer 几分钟才一次。
如果用 `shared_mutex`：

```
shared_mutex 的内部操作（简化）：
  lock_shared():
    atomic_add(&reader_count, 1)  // 原子 RMW 操作
    memory_fence                   // 确保看到 writer 的标记

  unlock_shared():
    atomic_sub(&reader_count, 1)
```

问题：`reader_count` 是全局争抢的 cache line。每个核执行 `atomic_add` 时，
必须先通过 MESI 协议把 cache line 拉到 Exclusive 状态。
100 个线程并发读 → 100 次 cache line bouncing → 每次约 50-100ns。
在 NUMA 架构下更差（跨 socket 约 200ns）。

#### 3.1.2 Thread-Local 锁方案

核心思路：**每个读线程只锁自己的 mutex，读线程之间零竞争**。

为什么这能消除 cache bouncing？shared_mutex 的问题在于所有线程争抢同一个
`reader_count` cache line。Thread-local 锁方案中，每个线程的 mutex 在不同的
cache line 上，lock 操作只涉及本线程独占的内存——没有跨核同步，没有 MESI 协议
开销。唯一需要跨核同步的是 `index_` 的 acquire load（一次 load 的代价远小于
一次 atomic RMW）。

```
DoublyBufferedData 数据布局:

  ┌─────────────────────────┐
  │  data_[0]  (前台)        │  ← 所有读线程通过 index_ 找到这里
  │  data_[1]  (后台)        │  ← 写线程修改这里
  │  index_ = 0 (atomic)    │  ← 标识哪个是前台
  │                          │
  │  wrappers_[] (weak_ptr)  │  ← 跟踪所有线程的 Wrapper
  │  modify_mutex_           │  ← 串行化写操作
  │  wrappers_mutex_         │  ← 保护 wrappers_ 数组
  └─────────────────────────┘

Thread-Local 存储（每线程独立，互不干扰）:

  Thread 1: { this → Wrapper1 { mutex_ } }    ← cache line A
  Thread 2: { this → Wrapper2 { mutex_ } }    ← cache line B
  Thread 3: { this → Wrapper3 { mutex_ } }    ← cache line C
  ...
```

**Read 流程**（约 25ns，几乎无争抢）：

```
Read(ScopedPtr* ptr):
  1. wrapper = GetOrCreateWrapper()     // thread_local 查找，O(1)
  2. wrapper->mutex_.lock()              // 只有本线程和写线程争抢
     // 正常情况：写线程不在 Modify → 无争抢 → 立即获得锁
  3. ptr->data_ = &data_[index_.load(acquire)]  // 读前台
  4. return
     // ScopedPtr 析构时调用 wrapper->mutex_.unlock()
```

关键点：第 2 步的 lock 几乎永远不会争抢。只有在写线程执行 Modify 的
"等待读线程完成"步骤时，才会有争抢——而写操作频率极低。

**性能对比**：

```
                shared_mutex           Thread-Local 锁
读操作开销      ~50-100ns + 争抢       ~25ns, 无争抢
读线程间竞争    有 (reader_count)      无
cache bouncing  严重 (O(#cores))       无
写操作开销      低                     O(#threads), 但低频可接受
适用场景        读写均衡               读远多于写 ← LALB 的情况
```

**Modify 流程**（O(#threads)，低频操作）：

```
Modify(fn):
  1. modify_mutex_.lock()              // 串行化所有写操作
  2. bg = !index_                       // 后台 buffer 索引
  3. fn(data_[bg])                      // 第一次修改后台
  4. index_.store(bg, release)          // 翻转前后台
     // 此后新来的 Read 看到新前台
  5. for wrapper in wrappers_:          // 遍历所有线程的锁
       wrapper->mutex_.lock()           // 等待该线程完成 Read
       wrapper->mutex_.unlock()         // 立即释放
     // ---- 此时保证：所有线程下次 Read 一定看到新 index_ ----
  6. fn(data_[!bg])                     // 第二次修改新后台
  7. modify_mutex_.unlock()
```

步骤 5 是核心。为什么 lock+unlock 就能保证线程看到新 index_？

```
情况 A：线程正在 Read（持有 wrapper->mutex_）
  → Modify 的 lock 阻塞，直到 Read 完成（ScopedPtr 析构释放锁）
  → 该线程下次 Read 时，步骤 3 必然看到新 index_（release-acquire 保证）

情况 B：线程没在 Read（mutex_ 空闲）
  → Modify 的 lock 立即成功
  → 该线程此刻不持有旧前台引用，下次 Read 时必然看到新 index_

两种情况的结论相同：遍历完所有 wrapper 后，旧前台不再有任何活跃读者。
```

#### 3.1.3 为什么用两把锁（modify_mutex_ + wrappers_mutex_）

```
modify_mutex_:  串行化写操作
wrappers_mutex_: 保护 wrappers_ 数组

不能合并的原因：
  Modify 持有锁的时间很长（需要等所有读线程完成 → 步骤 5）
  如果用同一把锁，新线程调用 GetOrCreateWrapper 需要往 wrappers_ 里 push
  → 被 Modify 阻塞 → 新线程连 Read 都做不了

分离后：
  GetOrCreateWrapper 只锁 wrappers_mutex_（短暂），不被 Modify 阻塞
  Modify 在步骤 5 期间才短暂锁 wrappers_mutex_（遍历数组）
```

#### 3.1.4 Per-Instance TLS

```
难点：
  C++ 的 thread_local 是 per-type 的，不是 per-instance 的。
  如果用 thread_local shared_ptr<Wrapper> w，
  两个 DoublyBufferedData<Servers> 实例会共享同一个 Wrapper → 完全错误。

brpc 的方案：WrapperTLSGroup
  - 全局 thread_local 数组池
  - 每个 DoublyBufferedData 实例分配唯一 key（类似 pthread_key_create）
  - Read 时: tls_array[key] → 得到本实例的 Wrapper
  - 优点：数组索引 O(1)，极快
  - 缺点：需要管理 key 的分配和回收

本项目的方案：thread_local unordered_map<this_ptr, Wrapper>
  - 每次 Read: map[this] → 得到本实例的 Wrapper
  - 优点：代码意图直观，无需管理 key 的分配和回收
  - 缺点：hash lookup 比数组索引慢（但在实际场景中可忽略）

析构时的陷阱：
  DoublyBufferedData 析构后，如果另一个实例恰好分配到相同的内存地址，
  线程的 TLS map 中残留的旧条目会指向一个未注册的 Wrapper。
  解决方案：析构函数清理当前线程的 TLS 条目（doubly_buffered_data.h:240-250）。
```

#### 3.1.5 ModifyWithForeground

AddServer 需要用 `ModifyWithForeground` 而非 `Modify`，原因：

```
Add 第一次调用（修改后台，fg = 旧前台）:
  fg 中没有新 id → 创建新 Weight 对象和 left_weight 条目

Add 第二次调用（修改新后台 = 原前台，fg = 第一次修改的）:
  fg 中已有新 id（第一次添加的）→ 直接复制 shared_ptr
  不需要重复创建 Weight → 前后台共享同一个 Weight 对象

如果用 Modify（没有 fg 参考）:
  第二次调用不知道 Weight 已经创建 → 会重复创建 → 前后台各有一份
  → Select 修改的是前台的 Weight，Modify 后翻转 → 修改丢失
```

### 3.2 MarkOld / ClearOld（Remove 时的前后台权值同步）

**源码**：`lalb/weight.h:26-53,91-102,130-136`, `lalb/weight.cc:68-101,216-233`, `lalb/weight_tree.cc:155-228`
**brpc 原版**：`locality_aware_load_balancer.cpp`

#### 3.2.1 问题

Remove 非末尾节点时，需要把末尾节点搬到被删位置（保持完全二叉树结构）。
但 DoublyBufferedData 的 fn 会被调用两次——两次之间前台读线程仍在运行。

用具体数值演示这个问题：

```
初始状态（4 个节点，权值如图）:

  前台树:              后台树（相同结构）:
      [0] A w=300          [0] A w=300
     /         \          /         \
  [1] B w=200  [2] C w=150   [1] B w=200  [2] C w=150
  /                           /
[3] D w=100                [3] D w=100

left_weight: [0].left=300  [1].left=100  （位置绑定，前后台共享指针）
total_weight = 750
```

现在要删除 B（index=1），需要把 D（index=3）搬到位置 1：

```
第一次修改后台：
  后台树变成:
      [0] A w=300
     /         \
  [1] D w=100  [2] C w=150    ← D 搬到了位置 1，原 B 被删
                               ← 位置 3 被 pop 掉

  翻转 → 这是新前台

但此刻旧前台（↓）可能还有读线程在用:
      [0] A w=300
     /         \
  [1] B w=0    [2] C w=150    ← B 已被 Disable，权值=0
  /
[3] D w=100                   ← 读线程仍然在位置 3 看到 D！
```

竞态问题：
- 旧前台的读线程对位置 3 的 D 调用 Feedback → `D.ResetWeight(index=3)`
- 权值变化 diff 被 `UpdateParentWeights(diff, 3)` 更新到位置 3 的父路径
- 但在新前台，D 已经在位置 1！
- **第二次修改如果不知道这些 diff，位置 1 的父路径就会缺失这些变化量**

#### 3.2.2 解决方案：用数值走一遍

```
第一次修改（后台 buffer）:

  1. B.Disable() → 返回 rm_weight=200（B 的权值变为 0）
  2. 搬移 D 到位置 1（只搬 server_id 和 weight 指针，left_weight 不动）
  3. D.MarkOld(3) → 记住 D 当前权值=100，标记 old_index_=3
     此后任何在 index=3 上的 ResetWeight 都会被追踪
  4. UpdateParentWeights(100 - 200 = -100, index=1)
     位置 1 的父路径权值减 100（从 B 的 200 变成 D 的 100）
     ⚠️ 不动位置 3 的父路径——前台还在用！

  --- 翻转 + 等待所有读线程切到新前台 ---

  在此期间，旧前台读线程的行为：
  - Select 可能命中 D（位置 3），调用 D.AddInflight → ResetWeight(index=3)
  - Feedback 可能回来，调用 D.Update → ResetWeight(index=3)
  - 假设产生了 3 次 diff：+5, -3, +8 → 累积到 D.old_diff_sum_ = +10
  - 同时 D 的权值从 100 变成了 110

第二次修改（新后台 = 原前台）:

  1. B.Disable() → 返回 0（已经 disabled）→ 进入第二次逻辑
  2. 搬移 D 到位置 1（同上）
  3. D.ClearOld() → 返回 {old_weight=100, accumulated_diff=10}
  4. UpdateParentWeights(+10, index=1)
     ← 把前台积累的 10 应用到新位置 1 的父路径
  5. UpdateParentWeights(-(100+10) = -110, index=3)
     ← 从旧位置 3 完全移除权值（100 本身 + 10 累积变化）
  6. 释放 B 的 Weight，释放位置 3 的 left_weight
  7. total_weight_ -= 200（从 LALB 层减去被删节点权值）

验证：
  修改前: 位置 3 的父路径上有 100+10=110 的权值
  修改后: 位置 3 清零（-110），位置 1 加了 10 → D 在新位置的权值正确
  total_weight 修正了 → 一切自洽
```

#### 3.2.3 ResetWeight 中的 diff 追踪

```cpp
// weight.cc:68-101 — ResetWeight()
int64_t Weight::ResetWeight(size_t index, int64_t now_us) {
  // ... 计算 new_weight（含 inflight delay 惩罚）...
  int64_t diff = new_weight - old_weight;

  // 关键一行：如果当前位置是被标记的旧位置，累积 diff
  if (old_index_ == index && diff != 0) {
    old_diff_sum_ += diff;
  }
  return diff;
}
```

为什么这行代码就够了？

- ResetWeight 在 Weight 的 mutex_ 保护下执行（AddInflight/Update 都先 lock）
- `old_index_` 由 MarkOld 设置，ClearOld 清除——都在同一个 mutex_ 下
- 所以 `old_diff_sum_` 的累加和读取是串行化的，没有竞态
- 代价：一个 if 判断 + 可能的加法 → 几乎为零

#### 3.2.4 Disable 的双重角色

brpc 巧妙利用 `Disable()` 的返回值区分第一次/第二次修改：

```
第一次调用: weight_ > 0 → Disable() 返回 200 → 进入 MarkOld 逻辑
第二次调用: weight_ = 0（第一次已 disable）→ Disable() 返回 0 → 进入 ClearOld 逻辑
```

不需要额外的标志位来区分，因为 Weight 对象在两次修改间是共享的（shared_ptr），
第一次 Disable 的效果在第二次时可见。

### 3.3 WeightTree（完全二叉树按权值查找）

**源码**：`lalb/weight_tree.h`, `lalb/weight_tree.cc`

#### 3.3.1 left_weight 为什么用指针

```cpp
struct TreeNode {
  uint64_t server_id;
  std::atomic<int64_t>* left_weight;  // 指针，不是值
  std::shared_ptr<Weight> weight;
};
```

三个原因：
1. **std::atomic 不可复制/移动** → 不能直接放 std::vector
2. **left_weight 和位置绑定，不跟着节点搬移**
   Remove 把 D 从位置 3 搬到位置 1 时：
   - server_id 和 weight 跟着 D 走
   - left_weight 留在原位（位置 1 的 left_weight 记录的是位置 1 的左子树权值和）
3. **前后台共享**
   前后台各有一个 TreeNode vector，但 left_weight 指针指向同一个 atomic
   → Select 在前台修改 left_weight，后台也能看到（指针解引用）

用 `std::deque<std::atomic<int64_t>>` 作为存储池：
- deque 保证元素不会因 push_back/pop_back 移动 → 指针稳定
- 这和 brpc 的 `std::deque<int64_t> _left_weights` 一致

#### 3.3.2 查找算法

每个节点有三个值：`left_weight`（左子树权值和）、`self_weight`（自身权值）。
右子树的权值不需要存，因为 `total = left + self + right`，right 可以推算。

```
                   [0] S1 w=300, left=300
                  /                      \
      [1] S2 w=200, left=100     [2] S3 w=150, left=0
      /
[3] S4 w=100, left=0

total_weight = 300 + 200 + 150 + 100 = 750

验证 left_weight:
  [0].left = 左子树总权值 = w[1] + w[3] = 200 + 100 = 300  ✓
  [1].left = 左子树总权值 = w[3] = 100                       ✓
  [2].left = 无左子节点 = 0                                   ✓
  [3].left = 无左子节点 = 0                                   ✓
```

**查找 dice=450 的过程**：

```
index=0: left=300, self=300
  dice=450 >= left=300? Yes → 不走左
  dice=450 < left+self=600? Yes → 命中节点 0（S1）
  传给 AddInflight 的 dice = 450 - 300 = 150

  AddInflight 检查: weight=300 >= 150? Yes → 选中 S1 ✓
```

**查找 dice=150 的过程**：

```
index=0: left=300, self=300
  dice=150 < left=300? Yes → 走左子树, index=1

index=1: left=100, self=200
  dice=150 >= left=100? Yes → 不走左
  dice=150 < left+self=300? Yes → 命中节点 1（S2）
  传给 AddInflight 的 dice = 150 - 100 = 50

  AddInflight 检查: weight=200 >= 50? Yes → 选中 S2 ✓
```

**查找 dice=700 的过程**：

```
index=0: left=300, self=300
  dice=700 >= left+self=600? Yes → 走右子树
  dice -= (300+300) = 100, index=2

index=2: left=0, self=150
  dice=100 >= left=0? Yes
  dice=100 < left+self=150? Yes → 命中节点 2（S3）
  传给 AddInflight 的 dice = 100 - 0 = 100

  AddInflight 检查: weight=150 >= 100? Yes → 选中 S3 ✓
```

整个过程 O(logN)，不需要任何全局锁。
left_weight 用 `memory_order_relaxed` 读取 → 性能极高。

#### 3.3.3 Select 中的"不一致性容忍"

Select 期间，left_weight、self_weight、total_weight 可能不一致：

```
场景：Feedback 刚更新了 S4 的权值 100→120（diff=+20）
      UpdateParentWeights 还没执行完——只更新了 [1].left（100→120），
      还没来得及更新 [0].left（仍然是 300）

此时 [0].left=300，但实际左子树总权值 = 200 + 120 = 320
→ dice 可能落在 300~320 的"空隙"里 → 走右子树但又越界
→ 循环退出，重试下一个 dice
```

这是 LALB 的设计哲学：
- 不追求强一致性（需要加锁），而是容忍短暂不一致（下次纠正）
- 最多重试 n 次，全部失败 → 随机兜底
- 在高并发下，偶尔的选择偏差远不如锁的代价大

### 3.4 base_weight（基础权值计算）

**源码**：`lalb/weight.cc:103-181`（Weight::Update）

```
base_weight = scaled_qps / avg_latency

其中:
  scaled_qps = (n-1) * 1000000 * WEIGHT_SCALE / time_window
  avg_latency = (bottom.latency_sum - top.latency_sum) / (n-1)
```

- 用大小为 128 的循环队列统计 QPS 和延时
- `WEIGHT_SCALE`=1008680231，让整数存储有足够精度（类似定点数，避免浮点运算）
- 所有权值有最小值保护（`kMinWeight=1000`），确保慢节点也能被探测到

为什么需要最小值保护？
如果一个节点权值降到 0，它永远不会被 Select 命中，
也就永远不会有 Feedback 来更新它的权值——**死锁**。
保留 1000 的最小权值，确保即使最慢的节点偶尔也会被探测到。

### 3.5 inflight delay（在途请求惩罚）

**源码**：`lalb/weight.cc:68-101`（ResetWeight）

这是 LALB 比简单加权轮询强大的关键能力：**不需要等超时就能发现变慢的节点**。

#### 原理

```
选择 server 时:
  inflight_sum_ += now           // 记录这个请求的开始时间
  inflight_count_++

反馈时:
  inflight_sum_ -= begin_time    // 移除已完成请求的开始时间
  inflight_count_--
```

`inflight_sum_ / inflight_count_` 是所有在途请求开始时间的平均值。
`inflight_delay = now - avg(begin_times)` 就是**在途请求的平均存活时间**。

#### 数值场景

```
正常情况（avg_latency = 10ms，punish_ratio = 1.5）:

  在途请求: 3 个，分别在 2ms、5ms、8ms 前发出
  inflight_delay = now - mean(now-2, now-5, now-8) = 5ms
  punish_threshold = 10ms * 1.5 = 15ms
  5ms < 15ms → 不惩罚，weight = base_weight

异常情况（server 变慢，请求堆积）:

  在途请求: 20 个，平均在 25ms 前发出
  inflight_delay = 25ms
  25ms > 15ms → 开始惩罚！
  weight = base_weight * 15 / 25 = base_weight * 0.6

  → 权值降低 40%，后续请求大幅减少
  → 不需要等任何请求超时（可能设置了 3s 超时）
  → 在变慢后几十毫秒就开始反应
```

惩罚公式：`weight = base_weight * avg_latency * punish_ratio / inflight_delay`

特点：
- 线性惩罚（inflight_delay 越大，权值越低）
- 实时响应（每次 Select/Feedback 都重算）
- 自恢复（请求完成后 inflight_delay 下降，权值自动回升）

### 3.6 错误处理

**源码**：`lalb/weight.cc:131-148`（Update 的 error 分支）

错误请求不会简单丢弃，而是用惩罚延时参与权值计算：

```
正常请求:
  直接 push 到循环队列，用实际延时参与 avg_latency 计算

错误请求:
  err_latency = actual_latency * kPunishErrorRatio(1.2)
  如果有 timeout: err_latency = max(err_latency, timeout_ms * 1000)
  累加到最近一个采样点的 latency_sum 上

效果:
  一次错误 → avg_latency 小幅上升 → base_weight 小幅下降
  连续多次错误 → latency_sum 持续累加 → avg_latency 趋近 timeout
  → base_weight 接近最小值 → 流量几乎完全转移到健康节点
  → 但保留 kMinWeight=1000 → 偶尔仍会探测该节点
  → 如果节点恢复 → 正常请求的低延时逐渐拉低 avg_latency → 权值回升
```

为什么用 `max(err_latency, timeout)` 而不是直接用 timeout？
因为实际延时可能大于 timeout（比如网络彻底断了，在应用层还没超时之前就报错了）。
取 max 确保惩罚不会低于超时级别。

## 4. 端到端 trace：跟一个请求走完全程

假设 3 个 server，当前状态：

```
树:
      [0] S1 w=500, left=350
     /                      \
  [1] S2 w=250, left=100   [2] S3 w=200, left=0
  /
[3] S4 w=100, left=0

total_weight_ = 1050
```

### 4.1 Select

```
1. LALB::Select()
   begin_time_us = NowUs() = 1000000（假设）
   total = total_weight_.load() = 1050

2. WeightTree::Select(1050, 1000000)
   DoublyBufferedData::Read(&s)
   → GetOrCreateWrapper() → 本线程的 Wrapper
   → Wrapper.mutex_.lock()     ← 几乎不争抢
   → s.data_ = &data_[index_.load(acquire)]  ← 获取前台

3. 生成 dice = random(0, 1050) = 380

4. 二叉树查找:
   index=0: left=350, self=500
   380 >= 350? Yes
   380 < 350+500=850? Yes → 命中 [0] S1
   dice_for_inflight = 380 - 350 = 30

5. S1.AddInflight(begin=1000000, index=0, dice=30):
   lock(S1.mutex_)
   ResetWeight(0, 1000000) → new_weight=480, diff=-20
   weight_=480 >= dice=30? Yes → chosen!
   inflight_sum_ += 1000000, inflight_count_++
   unlock(S1.mutex_)
   返回 {chosen=true, weight_diff=-20}

6. UpdateParentWeights(-20, index=0) → 无父节点，跳过

7. accumulated_diff = -20
   返回 {server_id=S1, weight_diff=-20, success=true}

8. LALB: total_weight_.fetch_add(-20) → total_weight_ = 1030
   返回 {server_id=S1, begin_time_us=1000000, success=true}

9. ScopedPtr 析构 → Wrapper.mutex_.unlock()
   ← 整个 Select 期间 thread-local 锁被持有，前台不会被翻转
```

### 4.2 RPC 执行 + Feedback

```
应用层: 向 S1 发送请求，10ms 后收到响应

1. LALB::Feedback(server_id=S1, begin_time=1000000, latency=10000, error=false)

2. WeightTree::Feedback(S1, 10000, 1000000, false, 0)
   DoublyBufferedData::Read(&s) → 获取前台快照

3. S1.Update(begin=1000000, error=false, timeout=0, index=0):
   end_time_us = NowUs() = 1010000
   latency = 1010000 - 1000000 = 10000 (10ms)

   lock(S1.mutex_)
   inflight_sum_ -= 1000000, inflight_count_--  ← 移除已完成请求

   QueuePush({latency_sum=..., end_time_us=1010000})
   重算 avg_latency_ 和 base_weight_

   ResetWeight(0, 1010000) → new_weight=510, diff=+30
   unlock(S1.mutex_)
   返回 diff=+30

4. UpdateParentWeights(+30, index=0) → 无父节点，跳过

5. LALB: total_weight_.fetch_add(+30) → total_weight_ = 1060

   ← Feedback 完成，S1 的权值从 480 更新到 510
   ← 下次 Select 时 S1 获得流量的概率略有变化
```

## 5. 关键设计取舍

| 决策 | 选择 | 为什么这么选 |
|------|------|-------------|
| 读写分离 | Thread-local 锁 | 读线程间零竞争——消除 shared_mutex 的 cache line bouncing，读 25ns vs 50-100ns |
| 权值存储 | 整数 + WEIGHT_SCALE | 避免浮点运算——atomic fetch_add 只支持整数，浮点需要 CAS 循环 |
| 一致性 | 最终一致 | 允许短暂的权值不一致——偶尔的选择偏差被 Feedback 纠正，远好于加锁的代价 |
| left_weight | atomic + relaxed | 不需要 acquire/release——relaxed 足够因为偏差会自纠，省掉每次的全核同步开销 |
| Remove 同步 | MarkOld/ClearOld | 零额外锁开销——只在已有 mutex 保护的 ResetWeight 里加一个 if |
| 最小权值 | 1000 | 避免"饿死"——权值为 0 的节点永远不会被选中，也就永远无法恢复 |
| 统计窗口 | 128 采样点 | 平衡灵敏度和稳定性——太小则权值震荡，太大则反应迟钝 |
| 错误惩罚 | 累加到最近采样点 | 渐进式——单次错误小幅降权，连续错误趋向 timeout 级惩罚，恢复后自动回升 |

## 6. 和 brpc 原实现的对照

| 模块 | brpc | 本项目 | 差异说明 |
|------|------|--------|---------|
| 双缓冲 | `DoublyBufferedData` + WrapperTLSGroup | `DoublyBufferedData` + thread_local map | 核心语义相同，TLS 管理方式不同 |
| Remove 同步 | MarkOld/ClearOld | MarkOld/ClearOld | 完全一致 |
| 权值计算 | Weight | Weight | 完全一致 |
| 二叉树查找 | Servers::weight_tree | Servers::weight_tree | 完全一致 |
| left_weight 存储 | `deque<int64_t>` + cast | `deque<atomic<int64_t>>` | 类型更安全 |
| Weight 生命周期 | raw pointer + manual delete | shared_ptr | 所有权语义更清晰，避免手动 delete |
| server map | FlatMap | unordered_map | 标准库，无额外依赖，接口一致 |
| 框架依赖 | Socket, ServerId, butil | 无依赖，纯 STL | 可独立编译运行，降低阅读门槛 |
