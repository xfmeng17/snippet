// ============================================================
// Weight 单元测试
// 测试权值计算器的各种行为
// ============================================================

#include "lalb/weight.h"

#include <chrono>
#include <thread>

#include "gtest/gtest.h"

namespace lalb {
namespace {

static int64_t NowUs() {
  std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::microseconds>(
             now.time_since_epoch())
      .count();
}

// ============================================================
// 单元测试：Weight 基本属性
// ============================================================

TEST(WeightTest, InitialWeight) {
  Weight w(1000);
  EXPECT_EQ(w.volatile_value(), 1000);
  EXPECT_FALSE(w.Disabled());
}

TEST(WeightTest, InitialWeightZero) {
  Weight w(0);
  EXPECT_EQ(w.volatile_value(), 0);
}

TEST(WeightTest, Disable) {
  Weight w(5000);
  int64_t old = w.Disable();
  EXPECT_EQ(old, 5000);
  EXPECT_TRUE(w.Disabled());
  EXPECT_EQ(w.volatile_value(), 0);
}

TEST(WeightTest, DisableReturnsZeroIfAlreadyZero) {
  Weight w(0);
  int64_t old = w.Disable();
  EXPECT_EQ(old, 0);
  EXPECT_TRUE(w.Disabled());
}

TEST(WeightTest, DoubleDisable) {
  Weight w(5000);
  w.Disable();
  int64_t old = w.Disable();
  // 已经 disabled，weight 已经是 0
  EXPECT_EQ(old, 0);
  EXPECT_TRUE(w.Disabled());
}

// ============================================================
// 单元测试：AddInflight
// ============================================================

TEST(WeightTest, AddInflightBasic) {
  Weight w(10000);
  int64_t now = NowUs();
  Weight::AddInflightResult r = w.AddInflight(now, 5000);
  EXPECT_TRUE(r.chosen);
}

TEST(WeightTest, AddInflightDiceTooLarge) {
  Weight w(100);
  int64_t now = NowUs();
  // dice > kMinWeight（ResetWeight 会把权值拉到 kMinWeight）
  Weight::AddInflightResult r = w.AddInflight(now, Weight::kMinWeight + 1);
  EXPECT_FALSE(r.chosen);
}

TEST(WeightTest, AddInflightDisabledNode) {
  Weight w(10000);
  w.Disable();
  int64_t now = NowUs();
  Weight::AddInflightResult r = w.AddInflight(now, 0);
  EXPECT_FALSE(r.chosen);
  EXPECT_EQ(r.weight_diff, 0);
}

// ============================================================
// 单元测试：MarkFailed
// ============================================================

TEST(WeightTest, MarkFailedReducesWeight) {
  Weight w(10000);
  int64_t diff = w.MarkFailed(2000);
  // base_weight 应该被降到 avg_weight=2000
  // 然后 ResetWeight 重算 → 新权值取 max(2000, kMinWeight)
  EXPECT_LE(diff, 0);  // 权值应该下降
}

TEST(WeightTest, MarkFailedNoEffectIfAlreadyLow) {
  Weight w(500);
  int64_t diff = w.MarkFailed(2000);
  // base_weight(500) <= avg_weight(2000)，不做任何事
  EXPECT_EQ(diff, 0);
}

// ============================================================
// 单元测试：Update（权值反馈更新）
// ============================================================

TEST(WeightTest, UpdateWithNormalRequest) {
  Weight w(Weight::kWeightScale);
  int64_t now = NowUs();

  // 模拟一次 inflight
  Weight::AddInflightResult r = w.AddInflight(now, 0);
  EXPECT_TRUE(r.chosen);

  // 等待一小段时间模拟延时
  std::this_thread::sleep_for(std::chrono::milliseconds(1));

  // 反馈正常结果
  int64_t latency = 1000;  // 1ms
  w.Update(latency, now, /*error=*/false, /*timeout_ms=*/0);

  // 权值应该被更新
  EXPECT_GT(w.volatile_value(), 0);
}

TEST(WeightTest, UpdateDisabledNodeIsNoop) {
  Weight w(10000);
  w.Disable();
  int64_t now = NowUs();
  int64_t diff = w.Update(1000, now, false, 0);
  EXPECT_EQ(diff, 0);
}

TEST(WeightTest, UpdateWithError) {
  Weight w(Weight::kWeightScale);
  int64_t now = NowUs();

  Weight::AddInflightResult r = w.AddInflight(now, 0);
  EXPECT_TRUE(r.chosen);

  std::this_thread::sleep_for(std::chrono::milliseconds(1));

  // 反馈错误结果
  w.Update(1000, now, /*error=*/true, /*timeout_ms=*/100);

  // 权值应该仍然有效（最小值保护）
  EXPECT_GE(w.volatile_value(), Weight::kMinWeight);
}

// ============================================================
// 单元测试：多次 Update 收敛
// ============================================================

TEST(WeightTest, MultipleUpdatesConverge) {
  Weight w(Weight::kWeightScale);

  // 模拟多次 RPC 反馈，延时固定 1ms
  for (int i = 0; i < 200; ++i) {
    int64_t now = NowUs();
    Weight::AddInflightResult r = w.AddInflight(now, 0);
    if (!r.chosen) continue;

    std::this_thread::sleep_for(std::chrono::microseconds(100));
    w.Update(1000, now, false, 0);
  }

  // 经过足够多的更新，权值应该稳定到一个正值
  EXPECT_GT(w.volatile_value(), 0);
  EXPECT_GT(w.avg_latency(), 0);
}

// ============================================================
// 单元测试：权值最小值保护
// ============================================================

TEST(WeightTest, MinWeightProtection) {
  Weight w(Weight::kMinWeight);

  int64_t now = NowUs();
  Weight::AddInflightResult r = w.AddInflight(now, 0);
  if (r.chosen) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    w.Update(1000000, now, false, 0);  // 非常高的延时
  }

  // 即使延时很高，权值也不会低于 kMinWeight
  EXPECT_GE(w.volatile_value(), Weight::kMinWeight);
}

}  // namespace
}  // namespace lalb
