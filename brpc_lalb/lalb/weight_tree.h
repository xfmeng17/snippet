#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include "lalb/weight.h"

namespace lalb {

// ============================================================
// 完全二叉树节点
// 注意：left_weight 用指针，因为 std::atomic 不可复制/移动，
// 且 left_weight 和位置绑定（不跟着节点移动）。
// 这和 brpc 原实现一致。
// ============================================================
struct TreeNode {
  uint64_t server_id;
  // 指向位置绑定的 left_weight（由 WeightTree 管理生命周期）
  std::atomic<int64_t>* left_weight;
  // 权值计算器（前后台共享）
  std::shared_ptr<Weight> weight;
};

// ============================================================
// WeightTree: 基于完全二叉树的按权值查找结构
//
// 对应 brpc 中的 Servers 内部类 + DoublyBufferedData。
// 简化版使用 shared_mutex 代替 DoublyBufferedData：
// - 读（select）拿 shared_lock，高并发
// - 写（add/remove）拿 unique_lock，低频
//
// 二叉树结构：
//   节点 i 的左子节点: 2*i + 1
//   节点 i 的右子节点: 2*i + 2
//   节点 i 的父节点:   (i-1) / 2
//   每个节点的 left_weight 记录左子树所有节点权值之和
// ============================================================
class WeightTree {
 public:
  WeightTree() = default;

  bool AddServer(uint64_t server_id, int64_t initial_weight);
  bool RemoveServer(uint64_t server_id);

  struct SelectResult {
    uint64_t server_id;
    int64_t weight_diff;
    bool success;
  };
  SelectResult Select(int64_t total_weight, int64_t begin_time_us);

  int64_t Feedback(uint64_t server_id, int64_t latency_us,
                   int64_t begin_time_us, bool error, int64_t timeout_ms);

  size_t Size() const;

  // 更新从 index 到根的所有父节点的 left_weight
  void UpdateParentWeights(int64_t diff, size_t index) const;

 private:
  // 分配一个新的 left_weight 条目
  std::atomic<int64_t>* AllocLeftWeight();
  // 释放最后一个 left_weight 条目
  void FreeLastLeftWeight();

  mutable std::shared_mutex mutex_;
  std::vector<TreeNode> nodes_;
  std::unordered_map<uint64_t, size_t> id_to_index_;
  // left_weight 存储池，用 deque 保证指针稳定
  std::deque<std::atomic<int64_t>> left_weights_;
};

}  // namespace lalb
