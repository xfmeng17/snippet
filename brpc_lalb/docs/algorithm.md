# LALB 算法与核心实现讲解

## 1. 核心思想：一句话版

> **以吞吐除以延时作为分流权值**，延时低的节点自然获得更多流量。

这不是简单的"按延时比例分流"。假设节点 A 延时 1ms，节点 B 延时 3ms，
LALB **不会**按 3:1 分流。而是：A 持续获得流量直到它的延时升高到和 B 持平，
或者没有更多流量了。分流比例是高度非线性的。

## 2. 直觉：为什么 QPS/Latency 能收敛

设两个节点权值 W1, W2，吞吐 QPS1, QPS2，延时 L1, L2。

权值公式：`W = QPS * SCALE / Latency`

稳态分析：
- 稳态时 QPS 和 W 成正比：`W1/W2 ≈ QPS1/QPS2`
- 代入公式：`W1/W2 = (QPS1/QPS2) * (L2/L1)`
- 两式联立 → `L1 ≈ L2`

**结论**：算法驱动所有节点的延时趋同。延时低的节点持续获得更多流量，
直到其延时被拉升到和其他节点一样。

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

Thread-Local 存储:

  Thread 1: { this → Wrapper1 { mutex_ } }
  Thread 2: { this → Wrapper2 { mutex_ } }
  Thread 3: { this → Wrapper3 { mutex_ } }
  ...
```

每个线程有自己独立的 `Wrapper`，里面有自己的 `mutex_`。

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

步骤 5 是核心：
- 如果线程正在 Read（持有 wrapper->mutex_），lock 会阻塞直到 Read 完成
- 如果线程没在 Read（mutex_ 空闲），lock 立即成功
- 无论哪种情况，该线程下次 Read 时一定会看到新的 index_

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

我们的简化方案：thread_local unordered_map<this_ptr, Wrapper>
  - 每次 Read: map[this] → 得到本实例的 Wrapper
  - 优点：代码简单，无需管理 key
  - 缺点：hash lookup 比数组索引慢（但在实际场景中可忽略）
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

**源码**：`lalb/weight.h:42-67`, `lalb/weight.cc:64-95`, `lalb/weight_tree.cc:89-165`
**brpc 原版**：`locality_aware_load_balancer.cpp:106-192`, `.h:89-90`

#### 3.2.1 问题

Remove 非末尾节点时，需要把末尾节点搬到被删位置（保持完全二叉树结构）。
但 DoublyBufferedData 的 fn 会被调用两次——两次之间前台读线程仍在运行。

```
初始状态：
  前台: [A, B, C, D]    （节点 0~3）
  后台: [A, B, C, D]    （和前台一样）

要删除 B（index=1），把 D（index=3）搬到位置 1：

第一次修改（后台）:
  后台变成: [A, D', C]   （D 搬到位置 1，位置 3 删除）
  翻转 → 新前台 = [A, D', C]

此时旧前台还有线程在读: [A, B, C, D]
这些线程的 Select 可能访问位置 3 的 D 节点，
调用 D.ResetWeight() 修改权值 → 但此时后台已经把 D 搬到位置 1
→ 父节点的 left_weight 更新到了旧位置 3 的路径上
→ 第二次修改时如果不补偿，位置 1 的父节点权值就不对了
```

#### 3.2.2 解决方案

```
第一次修改（后台 buffer）:
  1. w->Disable()      → 被删节点权值置 0，返回旧权值 rm_weight
  2. 搬移 D 到位置 1
  3. D->MarkOld(3)     → 记住 D 的当前权值，标记旧位置为 3
     之后如果 ResetWeight 在 index=3 被调用 → 累积 diff 到 old_diff_sum_
  4. UpdateParentWeights(add_weight - rm_weight, index=1)
     注意：不动位置 3 的父节点（前台还在用位置 3！）

  --- 翻转 + 等待所有读线程切换 ---

第二次修改（新后台 = 原前台）:
  1. w->Disable()      → 返回 0（已经 disabled），进入第二次逻辑
  2. 搬移 D 到位置 1
  3. D->ClearOld()     → 返回 {old_weight, accumulated_diff}
     old_weight: MarkOld 时 D 的权值
     accumulated_diff: 两次修改之间，前台读线程通过 ResetWeight(index=3) 产生的权值变化
  4. UpdateParentWeights(accumulated_diff, index=1)
     把前台产生的变化应用到新位置
  5. UpdateParentWeights(-(old_weight + accumulated_diff), index=3)
     从旧位置完全移除权值
  6. 释放被删节点的 Weight 和末尾 left_weight
```

#### 3.2.3 ResetWeight 中的 diff 追踪

```cpp
// weight.cc: ResetWeight()
int64_t Weight::ResetWeight(size_t index, int64_t now_us) {
  // ... 计算 new_weight ...
  int64_t diff = new_weight - old_weight;

  // 关键一行：如果当前位置是被标记的旧位置，累积 diff
  if (old_index_ == index && diff != 0) {
    old_diff_sum_ += diff;
  }
  return diff;
}
```

这一行代码解决了整个并发同步问题：
- 不需要锁住前台读线程
- 不需要阻塞 Select
- 只在 ResetWeight 里多一个 if 判断（已持有 Weight 的 mutex_）
- 零额外开销

#### 3.2.4 Disable 的双重角色

brpc 巧妙利用 `Disable()` 的返回值区分第一次/第二次修改：

```
第一次调用: weight_ > 0 → Disable() 返回 > 0 → 进入 MarkOld 逻辑
第二次调用: weight_ = 0（第一次已 disable）→ Disable() 返回 0 → 进入 ClearOld 逻辑
```

不需要额外的标志位来区分，因为 Weight 对象在两次修改间是共享的（shared_ptr），
第一次 Disable 的效果在第二次时可见。

### 3.3 WeightTree（完全二叉树按权值查找）

**源码**：`lalb/weight_tree.h`, `lalb/weight_tree.cc`
**brpc 原版**：`locality_aware_load_balancer.h:115-129`

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

```
                    [0] left=700
                   /            \
          [1] left=400      [2] left=150
          /        \        /        \
      [3] w=400  [4] w=200  [5] w=150  [6] w=100

查找 dice=550:
  index=0: left=700, 550 < 700 → 走左子树, index=1
  index=1: left=400, self=200
    550 >= 400 且 550 < 400+200=600 → 命中节点 1
    (实际上 dice-left = 550-400 = 150，传给 AddInflight 的 dice=150)

  index=0: left=700, 800 >= 700 → 不走左
    self=300, 800 >= 700+300=1000? No → 命中节点 0

整个过程 O(logN)，不需要任何锁。
left_weight 用 memory_order_relaxed 读取 → 性能极高。
```

#### 3.3.3 Select 中的"不一致性容忍"

```
Select 期间，left_weight / self_weight / total_weight 可能不一致：
- 另一个线程的 Feedback 刚更新了节点 3 的 weight，但还没更新父节点
- 或者更新了一半的父节点

可能导致：
- dice 落在"空隙"里 → 走到叶节点之外 → 重试
- AddInflight 发现权值不够 → 重试
- 最多重试 n 次，全部失败 → 随机兜底

这是 LALB 的设计哲学：
  不追求强一致性（需要加锁），而是容忍短暂不一致（下次纠正）。
  在高并发下，偶尔的选择偏差远不如锁的代价大。
```

### 3.4 base_weight（基础权值计算）

**源码**：`lalb/weight.cc:81-160`

```
base_weight = scaled_qps / avg_latency

其中:
  scaled_qps = (n-1) * 1000000 * WEIGHT_SCALE / time_window
  avg_latency = (bottom.latency_sum - top.latency_sum) / (n-1)
```

- 用大小为 128 的循环队列统计 QPS 和延时
- `WEIGHT_SCALE` 是放大系数（1008680231），让整数存储有足够精度
- 所有权值有最小值保护（`min_weight=1000`），确保慢节点也能被探测到

### 3.5 inflight delay（在途请求惩罚）

**源码**：`lalb/weight.cc:58-79`（ResetWeight）

```
选择 server 时:
  begin_time_sum += now
  begin_time_count++

反馈时:
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
SelectServer():
  1. DoublyBufferedData::Read(&s)           // 锁 thread-local mutex
  2. total = total_weight_.load(relaxed)     // 读全局权值
  3. dice = random(0, total)
  4. 二叉树查找 O(logN):
     left = left_weight->load(relaxed)       // 原子读，无锁
     self = weight->volatile_value()         // volatile 读
  5. AddInflight(begin_time, index, dice):
     lock(weight->mutex_)                    // 锁单个节点
     ResetWeight(index, now)                 // 重算权值（含 MarkOld 追踪）
     if weight >= dice: 选中
     inflight_sum += now; inflight_count++
  6. UpdateParentWeights(diff, index)        // atomic fetch_add(relaxed)
  7. ScopedPtr 析构，释放 thread-local 锁

Feedback():
  1. DoublyBufferedData::Read(&s)
  2. Weight::Update():
     lock(weight->mutex_)
     更新循环队列，重算 base_weight
     ResetWeight(index, now)                 // 含 MarkOld 追踪
  3. UpdateParentWeights(diff, index)
  4. total_weight_.fetch_add(diff, relaxed)

AddServer():
  DoublyBufferedData::ModifyWithForeground(Add):
    第一次(bg): 创建 Weight + left_weight，更新父节点
    翻转 + 等待读线程
    第二次(bg): 从 fg 复制指针

RemoveServer():
  DoublyBufferedData::Modify(Remove):
    第一次(bg): Disable + MarkOld + 搬移 + 更新父节点
    翻转 + 等待读线程
    第二次(bg): ClearOld + 修正父节点 + 释放资源
```

## 5. 关键设计取舍

| 决策 | 选择 | 原因 |
|------|------|------|
| 读写分离 | Thread-local 锁 | 读线程间零竞争，比 shared_mutex 快 4-10x |
| 权值存储 | 整数 + WEIGHT_SCALE | 避免浮点运算，原子操作友好 |
| 一致性 | 最终一致 | 允许短暂的权值不一致换取更高并发 |
| left_weight | atomic + relaxed | 不需要强一致性，偏差下次纠正 |
| Remove 同步 | MarkOld/ClearOld | 零额外开销，只在 ResetWeight 加一个 if |
| 最小权值 | 1000 | 慢节点也需要被探测，否则永远不知道它是否恢复 |
| 统计窗口 | 128 个采样点 | inflight delay 能及时纠偏，128 已够 |
| 错误处理 | 混合惩罚延时 | 错误次数越多越接近 timeout |

## 6. 和 brpc 原实现的对照

| 模块 | brpc | 本项目 | 差异说明 |
|------|------|--------|---------|
| 双缓冲 | `DoublyBufferedData` + WrapperTLSGroup | `DoublyBufferedData` + thread_local map | 核心语义相同，TLS 管理方式不同 |
| Remove 同步 | MarkOld/ClearOld | MarkOld/ClearOld | 完全一致 |
| 权值计算 | Weight | Weight | 完全一致 |
| 二叉树查找 | Servers::weight_tree | Servers::weight_tree | 完全一致 |
| left_weight 存储 | `deque<int64_t>` + cast | `deque<atomic<int64_t>>` | 类型更安全 |
| Weight 生命周期 | raw pointer + manual delete | shared_ptr | 简化内存管理 |
| server map | FlatMap | unordered_map | 性能稍差，但标准库 |
| 框架依赖 | Socket, ServerId, butil | 无依赖，纯 STL | 可独立编译运行 |
