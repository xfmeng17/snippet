#pragma once

// ============================================================
// DoublyBufferedData 概念说明
//
// brpc 中的 DoublyBufferedData 是 LALB 的核心并发基础设施。
// 这里不完整实现它，而是用 std::shared_mutex 简化替代。
//
// 原理回顾（见 docs/algorithm.md 3.1 节）：
// - 数据分前台和后台，读只访问前台（几乎无锁）
// - 写修改后台 → 切换前后台 → 逐个获取 thread-local 锁确保读线程切换
// - 读之间零竞争，写和读之间通过 thread-local 锁同步
//
// 本简化版的权衡：
// - 用 shared_mutex 替代 thread-local 锁机制
// - 读拿 shared_lock，写拿 unique_lock
// - 性能稍差（shared_mutex 有内部原子操作），但逻辑更清晰
// - 对于学习目的和中等规模场景完全够用
//
// 如果你需要极致性能（数千并发读线程），可以参考 brpc 原版
// butil/containers/doubly_buffered_data.h 实现 thread-local 版本。
// ============================================================
