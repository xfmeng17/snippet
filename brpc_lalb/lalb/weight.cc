#include "lalb/weight.h"

#include <algorithm>
#include <chrono>

namespace lalb {

// ============================================================
// 辅助：获取当前时间（微秒）
// ============================================================
static int64_t NowUs() {
  std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::microseconds>(
             now.time_since_epoch())
      .count();
}

// ============================================================
// 循环队列操作
// ============================================================

// 获取队首（最老的元素）
static TimeInfo* QueueTop(TimeInfo* items, int head) { return &items[head]; }

// 获取队尾（最新的元素）
static TimeInfo* QueueBottom(TimeInfo* items, int head, int size,
                             int capacity) {
  int idx = (head + size - 1) % capacity;
  return &items[idx];
}

// push，如果满了就覆盖最老的
static void QueuePush(TimeInfo* items, int* head, int* tail, int* size,
                      int capacity, const TimeInfo& info) {
  items[*tail] = info;
  *tail = (*tail + 1) % capacity;
  if (*size < capacity) {
    ++(*size);
  } else {
    *head = (*head + 1) % capacity;
  }
}

// ============================================================
// Weight 实现
// ============================================================

Weight::Weight(int64_t initial_weight)
    : weight_(initial_weight),
      base_weight_(initial_weight),
      inflight_sum_(0),
      inflight_count_(0),
      old_diff_sum_(0),
      old_index_(static_cast<size_t>(-1)),
      old_weight_(0),
      avg_latency_(0),
      queue_head_(0),
      queue_tail_(0),
      queue_size_(0) {}

// ============================================================
// ResetWeight: 根据 base_weight 和 inflight delay 重算有效权值
//
// 这个函数是 MarkOld 机制的关键路径。
// 当 old_index_ == index 时，说明当前 Weight 正在被 Remove 搬移，
// 前台读线程仍在旧位置访问它，我们需要记录权值变化量。
// ============================================================
int64_t Weight::ResetWeight(size_t index, int64_t now_us) {
  int64_t new_weight = base_weight_;

  // inflight delay 惩罚
  if (inflight_count_ > 0) {
    int64_t inflight_delay = now_us - inflight_sum_ / inflight_count_;
    int64_t punish_latency =
        static_cast<int64_t>(avg_latency_ * kPunishInflightRatio);
    if (inflight_delay >= punish_latency && avg_latency_ > 0) {
      new_weight = new_weight * punish_latency / inflight_delay;
    }
  }

  // 最小权值保护
  if (new_weight < kMinWeight) {
    new_weight = kMinWeight;
  }

  int64_t old_weight = weight_;
  weight_ = new_weight;
  int64_t diff = new_weight - old_weight;

  // ---- MarkOld 跟踪 ----
  // 如果 old_index_ 等于当前 index，说明这个 Weight 正在被 Remove 搬移：
  //   - 后台已经把它从旧位置（old_index_）搬到新位置
  //   - 但前台还在旧位置访问它
  //   - 前台的 Select/Feedback 调用 ResetWeight 时传入的 index 是旧位置
  //   - 我们把这些 diff 累积起来，第二次 Modify 时通过 ClearOld 取走
  if (old_index_ == index && diff != 0) {
    old_diff_sum_ += diff;
  }

  return diff;
}

int64_t Weight::Update(int64_t begin_time_us, bool error, int64_t timeout_ms,
                       size_t index) {
  int64_t end_time_us = NowUs();
  int64_t latency = end_time_us - begin_time_us;

  std::lock_guard<std::mutex> lock(mutex_);
  if (Disabled()) {
    return 0;
  }

  // 扣除 inflight 记录
  inflight_sum_ -= begin_time_us;
  --inflight_count_;

  if (latency <= 0) {
    return 0;
  }

  if (!error) {
    // 正常请求：新增采样点
    TimeInfo info = {latency, end_time_us};
    if (queue_size_ > 0) {
      info.latency_sum +=
          QueueBottom(queue_items_, queue_head_, queue_size_, kQueueCapacity)
              ->latency_sum;
    }
    QueuePush(queue_items_, &queue_head_, &queue_tail_, &queue_size_,
              kQueueCapacity, info);
  } else {
    // 错误请求：累加到最近一个采样点，使用惩罚延时
    int64_t err_latency = static_cast<int64_t>(latency * kPunishErrorRatio);
    // 如果有 timeout，混合计算
    if (timeout_ms > 0) {
      err_latency = std::max(err_latency, timeout_ms * 1000L);
    }
    if (queue_size_ > 0) {
      TimeInfo* bottom =
          QueueBottom(queue_items_, queue_head_, queue_size_, kQueueCapacity);
      bottom->latency_sum += err_latency;
      bottom->end_time_us = end_time_us;
    } else {
      TimeInfo info = {std::max(err_latency, timeout_ms * 1000L), end_time_us};
      QueuePush(queue_items_, &queue_head_, &queue_tail_, &queue_size_,
                kQueueCapacity, info);
    }
  }

  // 计算 QPS 和平均延时
  TimeInfo* top = QueueTop(queue_items_, queue_head_);
  int64_t top_time_us = top->end_time_us;
  int n = queue_size_;
  int64_t scaled_qps = kDefaultQps * kWeightScale;

  if (end_time_us > top_time_us) {
    if (n == kQueueCapacity || end_time_us >= top_time_us + 1000000L) {
      scaled_qps = (int64_t)(n - 1) * 1000000L * kWeightScale /
                   (end_time_us - top_time_us);
      if (scaled_qps < kWeightScale) {
        scaled_qps = kWeightScale;
      }
    }
    TimeInfo* bottom =
        QueueBottom(queue_items_, queue_head_, queue_size_, kQueueCapacity);
    avg_latency_ = (bottom->latency_sum - top->latency_sum) / (n - 1);
  } else if (n == 1) {
    TimeInfo* bottom =
        QueueBottom(queue_items_, queue_head_, queue_size_, kQueueCapacity);
    avg_latency_ = bottom->latency_sum;
  } else {
    return 0;
  }

  if (avg_latency_ == 0) {
    return 0;
  }

  base_weight_ = scaled_qps / avg_latency_;
  return ResetWeight(index, end_time_us);
}

Weight::AddInflightResult Weight::AddInflight(int64_t begin_time_us,
                                              size_t index, int64_t dice) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (Disabled()) {
    return {false, 0};
  }
  int64_t diff = ResetWeight(index, begin_time_us);
  if (weight_ < dice) {
    // inflight delay 惩罚使权值太低，不选这个节点
    return {false, diff};
  }
  inflight_sum_ += begin_time_us;
  ++inflight_count_;
  return {true, diff};
}

int64_t Weight::MarkFailed(size_t index, int64_t avg_weight) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (base_weight_ <= avg_weight) {
    return 0;
  }
  base_weight_ = avg_weight;
  return ResetWeight(index, 0);
}

int64_t Weight::Disable() {
  std::lock_guard<std::mutex> lock(mutex_);
  int64_t saved = weight_;
  base_weight_ = -1;
  weight_ = 0;
  return saved;
}

int64_t Weight::MarkOld(size_t index) {
  std::lock_guard<std::mutex> lock(mutex_);
  int64_t saved = weight_;
  old_weight_ = saved;
  old_diff_sum_ = 0;
  old_index_ = index;
  return saved;
}

std::pair<int64_t, int64_t> Weight::ClearOld() {
  std::lock_guard<std::mutex> lock(mutex_);
  int64_t old_weight = old_weight_;
  int64_t diff = old_diff_sum_;
  old_diff_sum_ = 0;
  old_index_ = static_cast<size_t>(-1);
  old_weight_ = 0;
  return std::make_pair(old_weight, diff);
}

}  // namespace lalb
