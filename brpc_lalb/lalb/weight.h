#pragma once

#include <cstdint>
#include <mutex>
#include <utility>

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
//
// ---- MarkOld/ClearOld 机制 ----
//
// 问题背景：Remove 非末尾节点时，需要把末尾节点搬到被删位置。
// 由于 DoublyBufferedData 的双缓冲机制，Remove 的修改函数会被调用两次：
//   第一次：修改后台 buffer（此时前台还在使用中）
//   第二次：翻转后，修改新后台（原前台，此时所有读线程已切换）
//
// 竞态问题：
//   第一次修改后台时，前台读线程仍在看旧的树结构。
//   前台的 Select/Feedback 可能正在修改末尾节点的权值（通过 ResetWeight）。
//   但在后台，末尾节点已经被搬到了新位置，父节点的 left_weight 也已更新。
//   如果不跟踪这些并发的权值变化，第二次修改时父节点的 left_weight 就会不一致。
//
// 解决方案：
//   MarkOld(old_index): 在第一次修改时调用
//     - 记住当前权值和位置
//     - 之后 ResetWeight 如果在 old_index 位置被调用，会累积 diff 到 _old_diff_sum
//
//   ClearOld(): 在第二次修改时调用
//     - 返回 {old_weight, accumulated_diff}
//     - old_weight: MarkOld 时的权值
//     - accumulated_diff: 两次修改之间前台读线程产生的权值变化
//     - 用这些信息修正第二次修改的父节点更新
//
// 这个设计的精妙之处：
//   不需要锁或阻塞前台读线程，只需在 ResetWeight 里多一个 if 判断，
//   就能完美跟踪并发的权值变化。代价是一个分支判断，几乎为零。
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
  // index: 该 Weight 在 weight_tree 中的位置（用于 MarkOld 跟踪）
  int64_t Update(int64_t latency_us, int64_t begin_time_us, bool error,
                 int64_t timeout_ms, size_t index);

  // 选择 server 时调用：记录 inflight 并重算权值
  struct AddInflightResult {
    bool chosen;
    int64_t weight_diff;
  };
  AddInflightResult AddInflight(int64_t begin_time_us, size_t index,
                                int64_t dice);

  // 标记节点失败，降低权值到平均值
  int64_t MarkFailed(size_t index, int64_t avg_weight);

  // 禁用权值（准备删除），返回旧权值
  int64_t Disable();
  bool Disabled() const { return base_weight_ < 0; }

  // ---- MarkOld/ClearOld: Remove 时的前后台权值同步 ----

  // 在第一次 Remove（后台 buffer）时调用
  // 记住当前权值和旧位置，之后 ResetWeight 会追踪变化量
  // index: 旧位置（即 weight_tree.size()，也就是搬移前的末尾位置）
  int64_t MarkOld(size_t index);

  // 在第二次 Remove（新后台 buffer）时调用
  // 返回 {old_weight, accumulated_diff}:
  //   old_weight: MarkOld 时记录的权值
  //   accumulated_diff: 两次修改之间，前台读线程通过 ResetWeight 产生的权值变化总和
  std::pair<int64_t, int64_t> ClearOld();

  // 获取当前权值（可能随时变化，volatile 语义）
  int64_t volatile_value() const { return weight_; }

  // 获取统计信息（用于调试）
  int64_t avg_latency() const { return avg_latency_; }
  int64_t base_weight() const { return base_weight_; }

 private:
  // 根据 base_weight 和 inflight delay 重算有效权值
  // index: 在 weight_tree 中的位置（用于 MarkOld diff 追踪）
  // now_us: 当前时间（微秒）
  // 返回权值变化量（new_weight - old_weight）
  int64_t ResetWeight(size_t index, int64_t now_us);

  // 当前有效权值（含 inflight 惩罚）
  // 用 volatile 防止编译器优化掉多线程下的读取
  volatile int64_t weight_;
  // 基础权值 = scaled_qps / avg_latency
  int64_t base_weight_;

  std::mutex mutex_;

  // inflight 统计：在途请求的 begin_time 之和与计数
  int64_t inflight_sum_;
  int inflight_count_;

  // ---- MarkOld/ClearOld 相关字段 ----
  // 两次 Modify 之间，前台读线程产生的权值变化累积
  int64_t old_diff_sum_;
  // 被标记的旧位置，(size_t)-1 表示未标记
  size_t old_index_;
  // MarkOld 时记录的权值
  int64_t old_weight_;

  // 平均延时
  int64_t avg_latency_;

  // 循环队列：存储最近的延时采样
  TimeInfo queue_items_[kQueueCapacity];
  int queue_head_;  // 最老的元素
  int queue_tail_;  // 下一个写入位置
  int queue_size_;  // 当前元素数
};

}  // namespace lalb
