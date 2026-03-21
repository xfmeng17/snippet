#pragma once

#include <cstdint>
#include <mutex>

namespace lalb {

// ============================================================
// 延时采样点
// ============================================================
struct TimeInfo {
  int64_t latency_sum;  // 累计延时（微秒）
  int64_t end_time_us;  // 采样结束时间
};

// ============================================================
// 权值计算器
// 对应 brpc 中 LocalityAwareLoadBalancer::Weight 内部类
//
// 核心公式: base_weight = scaled_qps / avg_latency
// 有效权值会受 inflight delay 惩罚：
//   当 inflight_delay > avg_latency * punish_ratio 时
//   weight = base_weight * avg_latency * punish_ratio / inflight_delay
// ============================================================
class Weight {
 public:
  // 权值放大系数，用整数存储但保留足够精度（类似定点数）
  static constexpr int64_t kWeightScale = 1008680231;
  // 统计队列大小
  static constexpr int kQueueCapacity = 128;
  // 默认 QPS（无数据时）
  static constexpr int64_t kDefaultQps = 1;
  // 最小权值，确保慢节点也能被探测
  static constexpr int64_t kMinWeight = 1000;
  // inflight delay 超过 avg_latency * kPunishRatio 时开始惩罚
  static constexpr double kPunishInflightRatio = 1.5;
  // 错误延时的惩罚倍数
  static constexpr double kPunishErrorRatio = 1.2;

  explicit Weight(int64_t initial_weight);

  // 反馈 RPC 结果，更新权值。返回权值变化量（diff）
  // latency_us: 本次 RPC 延时（微秒）
  // begin_time_us: RPC 发出时间
  // error: 是否出错
  // timeout_ms: 超时时间（毫秒），出错时用于惩罚计算
  int64_t Update(int64_t latency_us, int64_t begin_time_us, bool error,
                 int64_t timeout_ms);

  // 选择 server 时调用：记录 inflight 并重算权值
  // 返回 {是否被选中, 权值变化量}
  struct AddInflightResult {
    bool chosen;
    int64_t weight_diff;
  };
  AddInflightResult AddInflight(int64_t begin_time_us, int64_t dice);

  // 标记节点失败，降低权值到平均值
  int64_t MarkFailed(int64_t avg_weight);

  // 禁用权值（准备删除），返回旧权值
  int64_t Disable();
  bool Disabled() const { return base_weight_ < 0; }

  // 获取当前权值（可能随时变化）
  int64_t volatile_value() const { return weight_; }

  // 获取统计信息（用于调试）
  int64_t avg_latency() const { return avg_latency_; }
  int64_t base_weight() const { return base_weight_; }

 private:
  // 根据 base_weight 和 inflight delay 重算有效权值
  // 返回权值变化量
  int64_t ResetWeight(int64_t now_us);

  // 当前有效权值（含 inflight 惩罚）
  volatile int64_t weight_;
  // 基础权值 = scaled_qps / avg_latency
  int64_t base_weight_;

  std::mutex mutex_;

  // inflight 统计：在途请求的 begin_time 之和与计数
  int64_t inflight_sum_;
  int inflight_count_;

  // 平均延时
  int64_t avg_latency_;

  // 循环队列：存储最近的延时采样
  TimeInfo queue_items_[kQueueCapacity];
  int queue_head_;  // 最老的元素
  int queue_tail_;  // 下一个写入位置
  int queue_size_;  // 当前元素数
};

}  // namespace lalb
