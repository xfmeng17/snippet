# 架构对照

## 模块映射

```
brpc 原项目                              本项目（简化版）
─────────────────────────────────────────────────────────────
LocalityAwareLoadBalancer                LALB
├── Weight (内部类)                      Weight (独立类)
│   ├── _weight                          weight_
│   ├── _base_weight                     base_weight_
│   ├── _begin_time_sum/count            inflight_sum_/count_
│   ├── _avg_latency                     avg_latency_
│   ├── _time_q (BoundedQueue)           time_queue_ (circular_buffer)
│   ├── _old_* (前后台同步用)            （去掉，用锁保护）
│   └── _mutex                           mutex_
├── Servers (内部类)                     WeightTree (独立类)
│   ├── weight_tree (vector)             nodes_ (vector)
│   ├── server_map (FlatMap)             id_to_index_ (unordered_map)
│   └── UpdateParentWeights()            update_parent_weights()
├── DoublyBufferedData<Servers>          std::shared_mutex + 双 vector
│   ├── Read() → ScopedPtr              read_lock()
│   └── Modify()                         write_lock() + swap
├── _total (atomic)                      total_weight_ (atomic)
├── _left_weights (deque)                left_weights_ (在 nodes_ 内)
├── SelectServer()                       select()
├── Feedback()                           feedback()
├── AddServer/RemoveServer               add_server/remove_server
└── ServerId → SocketId mapping          直接用 uint64_t server_id
```

## 数据结构关系

```
LALB
 │
 ├── WeightTree (读写分离，shared_mutex 保护)
 │    │
 │    ├── nodes_[0]: { server_id, left_weight(atomic), weight_ptr }
 │    ├── nodes_[1]: { server_id, left_weight(atomic), weight_ptr }
 │    ├── nodes_[2]: { server_id, left_weight(atomic), weight_ptr }
 │    └── ...
 │
 ├── total_weight_ (atomic<int64_t>)
 │
 └── 每个 Weight 对象：
      ├── weight_ (当前有效权值，含 inflight 惩罚)
      ├── base_weight_ (QPS/latency 算出的基础权值)
      ├── inflight_sum_ / inflight_count_ (在途请求统计)
      ├── avg_latency_ (平均延时)
      └── time_queue_[128] (循环队列: { latency_sum, end_time })
```

## 线程模型

```
Thread 1 (读)──┐
Thread 2 (读)──┤  shared_lock → WeightTree.select()
Thread 3 (读)──┤  原子操作更新 left_weight, total
Thread N (读)──┘  Weight::mutex_ 保护单个节点的权值更新

Thread W (写)────  unique_lock → add/remove server
```
