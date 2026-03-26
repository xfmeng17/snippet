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
// 并发设计:
// - Select/Feedback（读路径）通过 DoublyBufferedData 的 thread-local 锁实现近乎无锁
// - AddServer/RemoveServer（写路径）通过 DoublyBufferedData::Modify 两次修改 + 翻转
// - total_weight_ 用 atomic 维护，被 Select 读取、Feedback 更新
//
// 使用流程:
//   LALB lb;
//   lb.AddServer(1);
//   lb.AddServer(2);
//
//   LALB::SelectResult result = lb.Select();
//   if (result.success) {
//       // 发送 RPC 到 result.server_id ...
//       lb.Feedback(result.server_id, result.begin_time_us,
//                   latency_us, error, timeout_ms);
//   }
// ============================================================
class LALB {
 public:
  LALB() : total_weight_(0) {}

  // 禁止拷贝/移动（含 WeightTree，不可拷贝）
  LALB(const LALB&) = delete;
  LALB& operator=(const LALB&) = delete;

  // 添加 server。成功返回 true，重复 id 返回 false。
  bool AddServer(uint64_t server_id);

  // 移除 server。成功返回 true，id 不存在返回 false。
  bool RemoveServer(uint64_t server_id);

  struct SelectResult {
    uint64_t server_id;     // 选中的 server（success=false 时无意义）
    int64_t begin_time_us;  // 选择时的时间戳（微秒），传给 Feedback 用于计算延时
    bool success;           // 是否成功选择（无 server 或全部 disabled 时为 false）
  };

  // 按权值随机选择一个 server。多线程可并发调用。
  SelectResult Select();

  // 反馈 RPC 结果，更新 server 的权值。
  // 每次 Select 成功后必须调用，否则 inflight 计数会泄漏。
  // begin_time_us: Select 返回的时间戳，用于计算延时和 inflight delay
  // latency_us: RPC 实际延时（微秒），error=true 时仍需提供（会被惩罚放大）
  // error: RPC 是否出错
  // timeout_ms: RPC 超时时间（毫秒），0 表示无超时限制。
  //             仅在 error=true 时生效，用于计算错误惩罚延时的下限。
  void Feedback(uint64_t server_id, int64_t begin_time_us, int64_t latency_us, bool error,
                int64_t timeout_ms = 0);

  // 当前 server 数量。
  size_t Size() const { return tree_.Size(); }

  // 当前全局权值总和（用 relaxed 读取，可能短暂不一致，仅供监控/调试）。
  int64_t TotalWeight() const { return total_weight_.load(std::memory_order_relaxed); }

 private:
  WeightTree tree_;
  std::atomic<int64_t> total_weight_;
};

}  // namespace lalb
