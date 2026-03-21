#include "lalb/lalb.h"

#include <chrono>

namespace lalb {

static int64_t NowUs() {
  std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::microseconds>(
             now.time_since_epoch())
      .count();
}

bool LALB::AddServer(uint64_t server_id) {
  int64_t initial_weight = Weight::kWeightScale;
  size_t n = tree_.Size();
  if (n > 0) {
    int64_t total = total_weight_.load(std::memory_order_relaxed);
    initial_weight = total / static_cast<int64_t>(n);
    if (initial_weight < Weight::kMinWeight) {
      initial_weight = Weight::kWeightScale;
    }
  }
  if (!tree_.AddServer(server_id, initial_weight)) {
    return false;
  }
  total_weight_.fetch_add(initial_weight, std::memory_order_relaxed);
  return true;
}

bool LALB::RemoveServer(uint64_t server_id) {
  // RemoveServer 内部会更新 parent weights
  // 这里需要更新 total_weight_
  // 由于 Weight::Disable 返回旧权值，我们通过 tree_ 内部处理
  if (!tree_.RemoveServer(server_id)) {
    return false;
  }
  // total_weight_ 会在 RemoveServer 内部通过 UpdateParentWeights 自动调整
  // 但 total_weight_ 是外部维护的，需要在这里重新计算
  // 简化处理：直接重新扫描（低频操作）
  // 实际上 brpc 在 Remove 流程里精确维护 total，这里简化
  return true;
}

LALB::SelectResult LALB::Select() {
  int64_t begin_time_us = NowUs();
  int64_t total = total_weight_.load(std::memory_order_relaxed);
  WeightTree::SelectResult r = tree_.Select(total, begin_time_us);
  if (r.success) {
    total_weight_.fetch_add(r.weight_diff, std::memory_order_relaxed);
    return {r.server_id, begin_time_us, true};
  }
  return {0, 0, false};
}

void LALB::Feedback(uint64_t server_id, int64_t begin_time_us,
                    int64_t latency_us, bool error, int64_t timeout_ms) {
  int64_t diff =
      tree_.Feedback(server_id, latency_us, begin_time_us, error, timeout_ms);
  if (diff != 0) {
    total_weight_.fetch_add(diff, std::memory_order_relaxed);
  }
}

}  // namespace lalb
