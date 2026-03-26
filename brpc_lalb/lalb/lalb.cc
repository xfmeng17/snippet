#include "lalb/lalb.h"

#include <chrono>

namespace lalb {

static int64_t NowUs() {
  std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
}

bool LALB::AddServer(uint64_t server_id) {
  int64_t weight = tree_.AddServer(server_id);
  if (weight <= 0) {
    return false;
  }
  total_weight_.fetch_add(weight, std::memory_order_relaxed);
  return true;
}

bool LALB::RemoveServer(uint64_t server_id) {
  int64_t weight = tree_.RemoveServer(server_id);
  if (weight <= 0) {
    return false;
  }
  total_weight_.fetch_sub(weight, std::memory_order_relaxed);
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

void LALB::Feedback(uint64_t server_id, int64_t begin_time_us, int64_t latency_us, bool error,
                    int64_t timeout_ms) {
  int64_t diff = tree_.Feedback(server_id, latency_us, begin_time_us, error, timeout_ms);
  if (diff != 0) {
    total_weight_.fetch_add(diff, std::memory_order_relaxed);
  }
}

}  // namespace lalb
