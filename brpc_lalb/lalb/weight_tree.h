#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <unordered_map>
#include <vector>

#include "lalb/doubly_buffered_data.h"
#include "lalb/weight.h"

namespace lalb {

// ============================================================
// 完全二叉树节点
//
// left_weight 用指针的原因：
// 1. std::atomic 不可复制/移动，不能直接放 std::vector
// 2. left_weight 和位置绑定，不跟着节点搬移
//    当 Remove 把末尾节点搬到被删位置时，只搬 server_id 和 weight，
//    left_weight 留在原位（因为它记录的是这个位置的左子树权值和，和位置有关，和节点无关）
// 3. 这和 brpc 原实现完全一致：butil::atomic<int64_t>* left
// ============================================================
struct TreeNode {
  uint64_t server_id;
  // 指向位置绑定的 left_weight（由 WeightTree 管理生命周期）
  std::atomic<int64_t>* left_weight;
  // 权值计算器（前后台共享同一个 Weight 对象）
  // 用 shared_ptr 是因为前后台 buffer 各有一份 TreeNode，但指向同一个 Weight
  std::shared_ptr<Weight> weight;
};

// ============================================================
// Servers: 前台/后台 buffer 的数据结构
//
// 对应 brpc 中的 Servers 内部类。
// DoublyBufferedData 会维护两份 Servers（前台和后台），
// Modify 操作修改后台 → 翻转 → 修改新后台。
// ============================================================
struct Servers {
  // 完全二叉树，按数组方式存储
  // 节点 i 的左子节点: 2*i + 1
  // 节点 i 的右子节点: 2*i + 2
  // 节点 i 的父节点:   (i-1) / 2
  std::vector<TreeNode> weight_tree;

  // server_id → 在 weight_tree 中的索引
  std::unordered_map<uint64_t, size_t> server_map;

  // 更新从 index 到根的所有父节点的 left_weight
  // 时间复杂度 O(logN)，不需要任何锁（left_weight 是原子操作）
  void UpdateParentWeights(int64_t diff, size_t index) const;
};

// ============================================================
// WeightTree: 基于完全二叉树的按权值查找结构
//
// 对应 brpc 中的 Servers + DoublyBufferedData<Servers>。
//
// 并发模型：
// - 读路径（Select/Feedback）: 通过 DoublyBufferedData::Read 获取前台快照
//   → 锁 thread-local mutex（几乎无争抢）
//   → 二叉树查找 O(logN)（无锁，left_weight 原子读取）
//   → 释放 thread-local mutex
//
// - 写路径（Add/Remove）: 通过 DoublyBufferedData::Modify
//   → 锁 modify_mutex（串行化写操作）
//   → 修改后台 buffer
//   → 翻转前后台
//   → 等待所有读线程切换
//   → 修改新后台
//
// 关键设计：
// 1. Weight 对象和 left_weight 指针在前后台之间共享
//    → Select 修改权值时，前后台都能看到（因为是指针/shared_ptr）
//    → 这允许读路径直接通过原子操作更新 left_weight，不需要写锁
//
// 2. Remove 非末尾节点使用 MarkOld/ClearOld 追踪并发权值变化
//    → 第一次修改（后台）: MarkOld 标记旧位置
//    → 翻转 + 等待读线程切换
//    → 第二次修改（新后台）: ClearOld 获取累积的 diff，修正父节点
// ============================================================
class WeightTree {
 public:
  WeightTree() = default;

  // 返回实际分配的初始权值，0 表示失败（重复 id）
  int64_t AddServer(uint64_t server_id);
  // 返回被移除节点的权值，0 表示失败（id 不存在）
  int64_t RemoveServer(uint64_t server_id);

  struct SelectResult {
    uint64_t server_id;
    int64_t weight_diff;
    bool success;
  };
  SelectResult Select(int64_t total_weight, int64_t begin_time_us);

  int64_t Feedback(uint64_t server_id, int64_t latency_us,
                   int64_t begin_time_us, bool error, int64_t timeout_ms);

  size_t Size() const;

 private:
  // ---- Add/Remove 的静态函数 ----
  // 这些是 DoublyBufferedData::Modify 的回调
  // brpc 也用静态函数 + this 指针的模式

  // Add: 需要前台参考（ModifyWithForeground）
  // 第一次调用: fg 中没有 id → 创建新 Weight 和 left_weight
  // 第二次调用: fg 中有 id → 复制指针（不重复创建）
  // out_weight: 第一次调用时写入实际分配的初始权值
  static bool Add(Servers& bg, const Servers& fg, uint64_t server_id,
                  WeightTree* self, int64_t* out_weight);

  // Remove: 不需要前台参考，但需要区分第一次/第二次
  // 通过 Weight::Disable() 的返回值区分：
  //   > 0: 第一次（后台）→ MarkOld
  //   = 0: 第二次（新后台）→ ClearOld + 清理资源
  // out_weight: 第一次调用时写入被移除节点的权值
  static bool Remove(Servers& bg, uint64_t server_id, WeightTree* self,
                     int64_t* out_weight);

  // ---- left_weight 管理 ----
  // 用 deque 保证指针稳定（vector resize 会导致指针失效）
  std::atomic<int64_t>* PushLeft();
  void PopLeft();

  DoublyBufferedData<Servers> db_servers_;
  std::deque<std::atomic<int64_t>> left_weights_;
};

}  // namespace lalb
