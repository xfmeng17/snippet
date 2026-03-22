# 架构对照

## 模块映射

```
brpc 原项目                              本项目
─────────────────────────────────────────────────────────────
LocalityAwareLoadBalancer                LALB
├── Weight (内部类)                      Weight (独立类)
│   ├── _weight (volatile)              weight_ (volatile)
│   ├── _base_weight                     base_weight_
│   ├── _begin_time_sum/count            inflight_sum_/count_
│   ├── _avg_latency                     avg_latency_
│   ├── _time_q (BoundedQueue)           queue_items_[] (循环队列)
│   ├── _old_diff_sum/_old_index/_old_weight  old_diff_sum_/old_index_/old_weight_
│   └── _mutex (butil::Mutex)            mutex_ (std::mutex)
├── Servers (内部类)                     Servers (独立 struct)
│   ├── weight_tree (vector<ServerInfo>) weight_tree (vector<TreeNode>)
│   ├── server_map (FlatMap)             server_map (unordered_map)
│   └── UpdateParentWeights()            UpdateParentWeights()
├── DoublyBufferedData<Servers>          DoublyBufferedData<Servers>
│   ├── Read() → ScopedPtr              Read() → ScopedPtr
│   ├── Modify(fn)                       Modify(fn)
│   ├── ModifyWithForeground(fn)         ModifyWithForeground(fn)
│   ├── WrapperTLSGroup (数组池 TLS)    thread_local unordered_map (per-instance TLS)
│   └── Wrapper (per-thread mutex)       Wrapper (per-thread mutex)
├── _total (atomic)                      total_weight_ (atomic)
├── _left_weights (deque<int64_t>)       left_weights_ (deque<atomic<int64_t>>)
├── Add(bg, fg, id, lb) [static]         Add(bg, fg, id, self) [static]
├── Remove(bg, id, lb) [static]          Remove(bg, id, self) [static]
├── SelectServer()                       Select()
├── Feedback()                           Feedback()
└── ServerId → SocketId mapping          直接用 uint64_t server_id
```

## 数据结构关系

```
LALB
 │
 ├── total_weight_ (atomic<int64_t>)
 │
 └── WeightTree
      │
      ├── DoublyBufferedData<Servers>
      │    ├── data_[0] (前台 Servers)
      │    │    ├── weight_tree[0..N-1]  (TreeNode vector)
      │    │    └── server_map           (id → index)
      │    ├── data_[1] (后台 Servers)
      │    │    └── (和前台结构相同，TreeNode 中的 Weight* 共享)
      │    ├── index_ (atomic, 标识哪个是前台)
      │    ├── wrappers_[] (所有线程的 Wrapper 弱引用)
      │    ├── modify_mutex_ (串行化写操作)
      │    └── wrappers_mutex_ (保护 wrappers_ 数组)
      │
      └── left_weights_ (deque<atomic<int64_t>>, 存储池)

前后台共享关系（关键！）:

  data_[0].weight_tree[i].weight  ──┐
                                     ├──→ 同一个 shared_ptr<Weight>
  data_[1].weight_tree[i].weight  ──┘

  data_[0].weight_tree[i].left_weight ──┐
                                          ├──→ 同一个 atomic<int64_t>*
  data_[1].weight_tree[i].left_weight ──┘
                                          ↓
                                    left_weights_[i]
```

## 线程模型

```
读路径（Select/Feedback）:

  Thread 1 ──→ Read()
    │  ①  thread_local map[this] → Wrapper1
    │  ②  Wrapper1.mutex_.lock()              ← 几乎不会争抢
    │  ③  data_[index_.load(acquire)]          ← 获取前台
    │  ④  二叉树查找 O(logN)                   ← 无锁
    │  ⑤  Weight::AddInflight()                ← 锁单个 Weight
    │  ⑥  UpdateParentWeights()                ← atomic fetch_add
    │  ⑦  Wrapper1.mutex_.unlock()             ← ScopedPtr 析构

  Thread 2 ──→ Read()
    │  ①  thread_local map[this] → Wrapper2    ← 和 Thread 1 无竞争
    │  ...

写路径（Add/Remove）:

  Thread W ──→ Modify(fn)
    │  ①  modify_mutex_.lock()                 ← 串行化
    │  ②  fn(data_[bg])                         ← 修改后台
    │  ③  index_.store(bg, release)             ← 翻转
    │  ④  for each wrapper:                     ← 等待所有读线程
    │       wrapper.mutex_.lock()               ← 可能阻塞
    │       wrapper.mutex_.unlock()
    │  ⑤  fn(data_[!bg])                        ← 修改新后台
    │  ⑥  modify_mutex_.unlock()

锁的层次（避免死锁）:
  modify_mutex_ → wrappers_mutex_ → Wrapper::mutex_ → Weight::mutex_
  读路径只用:                         Wrapper::mutex_ → Weight::mutex_
  → 读路径和写路径不会死锁
```

## Remove 时序图

```
时间 ──────────────────────────────────────────────────────→

写线程(Modify):
  ┌─ fn(bg) ─────────┐  ┌─ flip ─┐  ┌─ wait ──────┐  ┌─ fn(bg2) ─────┐
  │ Disable(B)        │  │ index_ │  │ lock each   │  │ Disable → 0   │
  │ Move D→pos1       │  │ = bg   │  │ wrapper     │  │ Move D→pos1   │
  │ D.MarkOld(3)      │  │        │  │ unlock      │  │ D.ClearOld()  │
  │ UpdateParents      │  │        │  │             │  │ → {old_w, diff}│
  │ (add_w-rm_w, 1)   │  │        │  │             │  │ UpdateParents  │
  └───────────────────┘  └────────┘  └─────────────┘  │ (diff, 1)     │
                                                        │ UpdateParents  │
                                                        │ (-(old+diff),3)│
                                                        │ PopLeft()      │
                                                        └───────────────┘

读线程(Select):
  ┌─────── 看到旧前台 ──────────────────────┐  ┌─── 看到新前台 ──→
  │ Select: D 在位置 3                       │  │ Select: D 在位置 1
  │ Feedback: D.Update(index=3)              │  │
  │   → ResetWeight(index=3)                 │  │
  │   → old_index_==3: diff 累积到 old_diff_sum │  │
  └──────────────────────────────────────────┘  └──────────────────
```
