#pragma once

#include <atomic>
#include <cstdint>

#include "lalb/weight_tree.h"

namespace lalb {

// ============================================================
// LALB: Locality-Aware Load Balancer
//
// 核心思想: 以 QPS/Latency 作为分流权值
// - 延时低的节点获得更多流量
// - 通过 inflight delay 快速发现故障节点
// - 完全二叉树 O(logN) 查找
//
// 使用流程:
//   LALB lb;
//   lb.AddServer(1);
//   lb.AddServer(2);
//   lb.AddServer(3);
//
//   auto result = lb.Select();
//   if (result.success) {
//       // 发送 RPC 到 result.server_id ...
//       lb.Feedback(result.server_id, result.begin_time_us,
//                   latency_us, error, timeout_ms);
//   }
// ============================================================
class LALB {
 public:
  LALB() : total_weight_(0) {}

  // 添加 server
  bool AddServer(uint64_t server_id);

  // 移除 server
  bool RemoveServer(uint64_t server_id);

  // 选择一个 server
  struct SelectResult {
    uint64_t server_id;     // 选中的 server
    int64_t begin_time_us;  // 选择时间（用于后续 Feedback）
    bool success;           // 是否成功选择
  };
  SelectResult Select();

  // 反馈 RPC 结果（必须在每次 Select 成功后调用）
  void Feedback(uint64_t server_id, int64_t begin_time_us, int64_t latency_us,
                bool error, int64_t timeout_ms = 0);

  // 获取 server 数量
  size_t Size() const { return tree_.Size(); }

  // 获取总权值
  int64_t TotalWeight() const {
    return total_weight_.load(std::memory_order_relaxed);
  }

 private:
  WeightTree tree_;
  std::atomic<int64_t> total_weight_;
};

}  // namespace lalb
