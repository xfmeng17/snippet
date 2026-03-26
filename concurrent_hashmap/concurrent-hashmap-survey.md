# C++ 并发 HashMap 方案调研

## 1. 概述

在高性能 C++ 服务中，线程安全的 HashMap 是基础组件。本文梳理业界主流的并发 HashMap 实现方案，从"读 wait-free + 写分片锁"到"完全 lock-free"，对比各方案的设计思路、适用场景和优缺点。

---

## 2. 主流方案分类

### 2.1 读 Wait-Free + 写分片锁（Hazard Pointer 方案）

**代表实现**：`folly::ConcurrentHashMap` (Facebook)

**核心思想**：
- 读路径通过 Hazard Pointer 保护，完全不加锁（wait-free）
- 写路径按 hash 值分片，每个分片一个 `std::mutex`，只锁对应的 bucket 区间
- 节点删除通过 Hazard Pointer 的 `Retire()` 延迟回收，保证读者不会访问已释放内存

**数据结构**：

```
┌─────────────────────────────────┐
│         BucketTable             │
│  ┌─────┐ ┌─────┐ ┌─────┐      │
│  │Shard│ │Shard│ │Shard│ ...   │  <- 每个 shard 一个 mutex（写互斥）
│  └──┬──┘ └──┬──┘ └──┬──┘      │
│     v       v       v          │
│  [Bucket] [Bucket] [Bucket]    │  <- 每个 bucket 一条链表
│   Node->Node->Node->null       │  <- 节点继承 HazptrObject
│                                 │
│  读: Hazard Pointer 保护遍历    │  <- wait-free
│  写: lock(shard_mutex) + 修改   │  <- 分片锁
│  删: 摘链 + Retire() 延迟回收   │  <- Hazard Pointer 回收
└─────────────────────────────────┘
```

**API 示例**：

| 方法 | 语义 | 锁 |
|---|---|---|
| `Get(k, &v)` | 查找 key，返回 value | **无锁**（Hazard Pointer） |
| `Insert(k, v)` | 插入，key 已存在返回 false | 分片锁 |
| `Erase(k)` | 删除，通过 Retire() 延迟回收 | 分片锁 |
| `GetOrInsert(k, v, &exist)` | 存在则读取，不存在则插入 | 分片锁 |
| `InsertOrAssign(k, v)` | 存在则替换，不存在则插入 | 分片锁 |

**优点**：
- 读路径性能极好（~10ns），适合读多写少
- 实现复杂度适中，正确性较易保证
- 写路径不同 shard 之间不互相阻塞

**缺点**：
- 需要 Hazard Pointer 基础设施
- 对象释放可能延迟
- 通常不支持 Rehash（bucket 数量固定）

### 2.2 完全 Lock-Free（CAS + 标记删除）

**代表实现**：基于 Michael (2002) / Harris (2001) 论文的 lock-free linked-list hashmap

**核心思想**：
- 读路径直接遍历链表，通过指针低位标记跳过已删除节点
- 写路径用 CAS（Compare-And-Swap）原子操作插入/删除，无需任何锁
- 删除分两阶段：先对 next 指针打标记（逻辑删除），再 CAS 摘除（物理删除）
- 使用 pointer + version 解决 ABA 问题

**关键技术**：

```cpp
// 两阶段标记删除
// 1. 逻辑删除：在 next 指针最低位打标记
CAS(&curr->next, CLEAR_MASK(next), SET_DEL_MASK(next));

// 2. 物理摘除：将前驱的 next 指向被删节点的下一个
CAS(&prev->next, curr, CLEAR_MASK(next));

// 3. 延迟回收：等安全时刻再释放内存
DelayFree(curr);
```

**ABA 防护**：

```cpp
struct NodePointerPack {
    NodeType* pointer;
    uint64_t  ver;       // 版本号，每次修改 +1
};
// CAS 时同时比较 pointer 和 ver，需要 128-bit CAS (cmpxchg16b)
```

**优点**：
- 读写都不加锁，理论上吞吐量最高
- 无死锁风险

**缺点**：
- 删除逻辑极其复杂（百行级别），竞态分析困难
- 依赖 128-bit CAS（平台相关）
- 内存回收需要外部配合（定期调用 Recycle）
- 容易有微妙的并发 bug

### 2.3 开放寻址 + CAS（Junction 方案）

**代表实现**：[junction](https://github.com/preshing/junction) (Jeff Preshing)

Junction 提供四种并发 HashMap 变体：

| 变体 | 特点 |
|---|---|
| **ConcurrentMap_Crude** | 最简单的 lock-free，relaxed memory order |
| **ConcurrentMap_Linear** | 线性探测，支持 resize 和删除 |
| **ConcurrentMap_Leapfrog** | 类 hopscotch hashing 探测，高填充率下更高效 |
| **ConcurrentMap_Grampa** | Leapfrog 升级版，大表拆分为多个小子表 |

**核心特点**：
- **开放寻址**（非链表桶），cache 更友好
- **QSBR 内存回收**：线程在空闲时声明"我不再引用旧数据"，比 Hazard Pointer 更轻量
- **可逆 hash 函数**：从 hash 值能还原原始 key，节省内存
- **协作式 Resize**：受 Cliff Click (2007) 启发，新旧表共存，各线程协作迁移
- **限制**：只支持整数和裸指针作为 key

**参考博客**：
- [The World's Simplest Lock-Free Hash Table](https://preshing.com/20130605/the-worlds-simplest-lock-free-hash-table/)
- [New Concurrent Hash Maps for C++](https://preshing.com/20160201/new-concurrent-hash-maps-for-cpp/)
- [A Resizable Concurrent Map](https://preshing.com/20160222/a-resizable-concurrent-map/)

### 2.4 纯分片锁（最简单方案）

**设计**：将整个 map 分成 N 个 shard，每个 shard 一个 `std::mutex` + `std::unordered_map`。

```cpp
struct alignas(hardware_destructive_interference_size) Shard {
    std::mutex lock;
    std::unordered_map<K, V> map;
};
std::unique_ptr<Shard[]> shards_;  // 例如 128 个 shard
```

**优点**：实现最简单，正确性显而易见
**缺点**：读路径也要加锁，高并发读下成为瓶颈

### 2.5 Per-Bucket 读写锁（固定容量场景）

对于容量固定、不支持 resize 的场景，一种务实的选择是每个 bucket 一个读写锁：

**优点**：
- 实现简单，正确性容易保证
- 不需要 Hazard Pointer 等复杂内存回收机制
- 内存及时释放

**问题**：
- `std::shared_mutex` 的 read lock 本身不轻——两次 atomic RMW（加锁 reader_count+1、解锁 -1）
- 热点 bucket 的 reader count 缓存行会被多线程反复修改（cache line bouncing）
- Linux 上 `std::shared_mutex` 默认 writer-preferring，写者等待时新读者会被阻塞，可能产生延迟毛刺
- 对比 Hazard Pointer：HP 的读者写自己的 thread-local entry（无跨线程竞争），rwlock 的读者争抢同一个 lock state

```
Hazard Pointer:  每个读者写自己的 TLS entry -> 缓存行独立，不 bounce
                 [Thread1.entry] [Thread2.entry] [Thread3.entry]

Per-bucket rwlock: 所有读同一 bucket 的线程竞争同一个 reader count
                 [bucket.rwlock.count] <- 所有读者都要 atomic RMW 这个
```

**优化建议**：
- 用轻量级 rwlock（基于 `atomic<uint32_t>`）替代 `std::shared_mutex`
- bucket 结构 `alignas(64)` 避免 false sharing
- 对于 trivially copyable 的小 value，可用 SeqLock 替代——读端完全无原子写操作

---

## 3. Hazard Pointer 实现对比

Hazard Pointer 是方案 2.1 的核心基础设施。以下对比 folly（Facebook 开源）和一个典型的简化实现：

### 3.1 读路径（几乎一致）

两者都采用经典模式：store -> light barrier -> load -> compare

```cpp
// 典型实现
bool TryKeep(T** ptr, std::atomic<T*>* src) {
    auto p = *ptr;
    entry_->ExposePtr(p);                // 1. 将指针写入 hazard slot
    AsymmetricBarrierLight();            // 2. 轻量内存屏障（读端优化）
    *ptr = src->load(memory_order_acquire); // 3. 重新读取源指针
    if (p != *ptr) {                     // 4. 检查是否变化
        entry_->ExposePtr(nullptr);
        return false;                    // 变了，重试
    }
    return true;
}
```

读端使用轻量屏障（`AsymmetricBarrierLight`），写端使用重屏障（`AsymmetricBarrierHeavy`），这是一个重要的性能优化——读多写少时绝大多数开销集中在读端，轻量屏障显著降低读端延迟。

### 3.2 回收机制（差距最大）

| 维度 | folly | 简化实现 |
|---|---|---|
| **触发策略** | 批量回收（retired 数超阈值时触发） | 简单计数（每 N 次 Retire 触发一次） |
| **回收粒度** | Cohort 分组，相关对象一起回收 | 全局一锅端 |
| **同步回收** | 支持（`cleanup_cohort_tag` 保证返回前全部释放） | 不支持 |
| **查找保护指针** | `F14FastSet`（高性能 hash set） | `std::unordered_set` |
| **定时兜底** | 有 | 有（如 100ms 定时器） |
| **线程本地缓存** | 完善（批量预分配、flush） | 简单栈式缓存（固定 8 个槽） |

### 3.3 被保护对象

| 维度 | folly | 简化实现 |
|---|---|---|
| **基类** | `hazptr_obj`（reclaim 函数指针 + cohort_tag） | `Object`（virtual DestroySelf） |
| **Cohort 支持** | 有（相关对象分组，隔离回收） | 无 |
| **Linked 对象** | 支持（链式结构协同回收） | 不支持 |
| **标准化** | C++26 `std::hazard_pointer` 参考实现 (P2530) | 无 |

---

## 4. 方案选型指南

| 场景 | 推荐方案 |
|---|---|
| 读 QPS 不高（< 百万/s），追求简单 | 纯分片锁 或 per-bucket rwlock |
| 读多写少，要求读延迟低且稳定 | Hazard Pointer + 分片锁（folly 方案） |
| 读极热、几乎不删除、容量可预估 | `folly::AtomicHashMap` 或开放寻址 CAS |
| key 只有整数/指针，追求极致吞吐 | junction Leapfrog |
| 需要完全 lock-free（包括写路径） | CAS + 标记删除（实现复杂，慎用） |

---

## 5. 业界主要开源实现

| 库 | 类型 | 特点 |
|---|---|---|
| [folly::ConcurrentHashMap](https://github.com/facebook/folly) | 读 HP 无锁 + 写分片锁 | 生产级，支持 rehash/iterator |
| [folly::AtomicHashMap](https://github.com/facebook/folly) | 完全 lock-free 开放寻址 | 不支持删除和 rehash，容量固定 |
| [junction](https://github.com/preshing/junction) | 完全 lock-free 开放寻址 | 三种策略可选，实验性质 |
| [libcds](https://github.com/khizmax/libcds) | 多种 lock-free 容器 | Michael/Harris/Split-ordered list |
| [Intel TBB concurrent_hash_map](https://github.com/oneapi-src/oneTBB) | per-bucket reader-writer lock | 老牌工业实现 |
| [Boost.Concurrent (1.83+)](https://www.boost.org/) | 分段锁 | `boost::concurrent_flat_map` |
| [liburcu cds_lfht](https://liburcu.org/) | RCU + lock-free | Linux 内核 RCU 的用户态实现 |

---

## 6. 参考资料

- Maged Michael, *High Performance Dynamic Lock-Free Hash Tables and List-Based Sets*, SPAA 2002
- Timothy Harris, *A Pragmatic Implementation of Non-Blocking Linked-Lists*, DISC 2001
- Cliff Click, *A Lock-Free Wait-Free Hash Table that Reclaims Memory*, 2007
- [P2530R3 - Why Hazard Pointers Should be in C++26](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p2530r3.pdf)
- [Jeff Preshing - The World's Simplest Lock-Free Hash Table](https://preshing.com/20130605/the-worlds-simplest-lock-free-hash-table/)
- [Jeff Preshing - New Concurrent Hash Maps for C++](https://preshing.com/20160201/new-concurrent-hash-maps-for-cpp/)
- [Jeff Preshing - A Resizable Concurrent Map](https://preshing.com/20160222/a-resizable-concurrent-map/)
