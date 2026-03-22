// ============================================================
// Weight 单元测试
// 测试权值计算器的各种行为，包括 MarkOld/ClearOld 机制
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
  EXPECT_EQ(old, 0);
  EXPECT_TRUE(w.Disabled());
}

// ============================================================
// 单元测试：AddInflight
// ============================================================

TEST(WeightTest, AddInflightBasic) {
  Weight w(10000);
  int64_t now = NowUs();
  Weight::AddInflightResult r = w.AddInflight(now, 0, 5000);
  EXPECT_TRUE(r.chosen);
}

TEST(WeightTest, AddInflightDiceTooLarge) {
  Weight w(100);
  int64_t now = NowUs();
  Weight::AddInflightResult r = w.AddInflight(now, 0, Weight::kMinWeight + 1);
  EXPECT_FALSE(r.chosen);
}

TEST(WeightTest, AddInflightDisabledNode) {
  Weight w(10000);
  w.Disable();
  int64_t now = NowUs();
  Weight::AddInflightResult r = w.AddInflight(now, 0, 0);
  EXPECT_FALSE(r.chosen);
  EXPECT_EQ(r.weight_diff, 0);
}

// ============================================================
// 单元测试：MarkFailed
// ============================================================

TEST(WeightTest, MarkFailedReducesWeight) {
  Weight w(10000);
  int64_t diff = w.MarkFailed(0, 2000);
  EXPECT_LE(diff, 0);
}

TEST(WeightTest, MarkFailedNoEffectIfAlreadyLow) {
  Weight w(500);
  int64_t diff = w.MarkFailed(0, 2000);
  EXPECT_EQ(diff, 0);
}

// ============================================================
// 单元测试：Update（权值反馈更新）
// ============================================================

TEST(WeightTest, UpdateWithNormalRequest) {
  Weight w(Weight::kWeightScale);
  int64_t now = NowUs();

  Weight::AddInflightResult r = w.AddInflight(now, 0, 0);
  EXPECT_TRUE(r.chosen);

  std::this_thread::sleep_for(std::chrono::milliseconds(1));

  w.Update(now, false, 0, 0);
  EXPECT_GT(w.volatile_value(), 0);
}

TEST(WeightTest, UpdateDisabledNodeIsNoop) {
  Weight w(10000);
  w.Disable();
  int64_t now = NowUs();
  int64_t diff = w.Update(now, false, 0, 0);
  EXPECT_EQ(diff, 0);
}

TEST(WeightTest, UpdateWithError) {
  Weight w(Weight::kWeightScale);
  int64_t now = NowUs();

  Weight::AddInflightResult r = w.AddInflight(now, 0, 0);
  EXPECT_TRUE(r.chosen);

  std::this_thread::sleep_for(std::chrono::milliseconds(1));
  w.Update(now, true, 100, 0);
  EXPECT_GE(w.volatile_value(), Weight::kMinWeight);
}

// ============================================================
// 单元测试：多次 Update 收敛
// ============================================================

TEST(WeightTest, MultipleUpdatesConverge) {
  Weight w(Weight::kWeightScale);

  for (int i = 0; i < 200; ++i) {
    int64_t now = NowUs();
    Weight::AddInflightResult r = w.AddInflight(now, 0, 0);
    if (!r.chosen) continue;

    std::this_thread::sleep_for(std::chrono::microseconds(100));
    w.Update(now, false, 0, 0);
  }

  EXPECT_GT(w.volatile_value(), 0);
  EXPECT_GT(w.avg_latency(), 0);
}

// ============================================================
// 单元测试：权值最小值保护
// ============================================================

TEST(WeightTest, MinWeightProtection) {
  Weight w(Weight::kMinWeight);

  int64_t now = NowUs();
  Weight::AddInflightResult r = w.AddInflight(now, 0, 0);
  if (r.chosen) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    w.Update(now, false, 0, 0);
  }

  EXPECT_GE(w.volatile_value(), Weight::kMinWeight);
}

// ============================================================
// 单元测试：MarkOld / ClearOld
//
// 测试 Remove 时的双缓冲权值追踪机制
// ============================================================

TEST(WeightTest, MarkOldClearOldBasic) {
  Weight w(5000);

  // MarkOld: 记住当前权值和位置
  int64_t marked = w.MarkOld(3);
  EXPECT_EQ(marked, 5000);

  // ClearOld: 没有权值变化，diff 应该为 0
  std::pair<int64_t, int64_t> p = w.ClearOld();
  EXPECT_EQ(p.first, 5000);   // old_weight
  EXPECT_EQ(p.second, 0);     // accumulated_diff
}

TEST(WeightTest, MarkOldTracksDiffAtOldIndex) {
  // 模拟场景：
  // 1. Weight 初始权值 kWeightScale
  // 2. MarkOld(index=5) → 开始追踪
  // 3. 多次 AddInflight + Update → 权值变化，ResetWeight 在 index=5 被调用
  // 4. ClearOld → 获取累积的 diff
  Weight w(Weight::kWeightScale);

  // 先建立一些统计数据
  for (int i = 0; i < 50; ++i) {
    int64_t now = NowUs();
    Weight::AddInflightResult r = w.AddInflight(now, 5, 0);
    if (r.chosen) {
      std::this_thread::sleep_for(std::chrono::microseconds(100));
      w.Update(now, false, 0, 5);
    }
  }

  int64_t weight_before_mark = w.volatile_value();

  // MarkOld: 开始追踪 index=5 上的变化
  int64_t marked = w.MarkOld(5);
  EXPECT_EQ(marked, weight_before_mark);

  // 继续一些请求，权值会因 inflight delay 变化
  int64_t expected_diff = 0;
  for (int i = 0; i < 20; ++i) {
    int64_t now = NowUs();
    // AddInflight 在 index=5 会调用 ResetWeight(5, now)
    Weight::AddInflightResult r = w.AddInflight(now, 5, 0);
    expected_diff += r.weight_diff;
    if (r.chosen) {
      std::this_thread::sleep_for(std::chrono::microseconds(50));
      int64_t diff = w.Update(now, false, 0, 5);
      expected_diff += diff;
    }
  }

  // ClearOld: 应该能拿到累积的 diff
  std::pair<int64_t, int64_t> p = w.ClearOld();
  EXPECT_EQ(p.first, weight_before_mark);  // old_weight
  EXPECT_EQ(p.second, expected_diff);       // accumulated_diff
}

TEST(WeightTest, MarkOldIgnoresDiffAtDifferentIndex) {
  // MarkOld(index=5)，但 ResetWeight 在 index=3 被调用
  // → diff 不应该被追踪
  Weight w(Weight::kWeightScale);

  // 建立统计数据
  for (int i = 0; i < 50; ++i) {
    int64_t now = NowUs();
    Weight::AddInflightResult r = w.AddInflight(now, 3, 0);
    if (r.chosen) {
      std::this_thread::sleep_for(std::chrono::microseconds(100));
      w.Update(now, false, 0, 3);
    }
  }

  // MarkOld 在 index=5
  w.MarkOld(5);

  // 继续请求，但 index=3（不是 old_index）
  for (int i = 0; i < 10; ++i) {
    int64_t now = NowUs();
    Weight::AddInflightResult r = w.AddInflight(now, 3, 0);
    if (r.chosen) {
      std::this_thread::sleep_for(std::chrono::microseconds(50));
      w.Update(now, false, 0, 3);
    }
  }

  // ClearOld: diff 应该为 0（没有在 index=5 上的变化）
  std::pair<int64_t, int64_t> p = w.ClearOld();
  EXPECT_EQ(p.second, 0);
}

TEST(WeightTest, ClearOldResetsState) {
  Weight w(5000);

  w.MarkOld(3);
  std::pair<int64_t, int64_t> p1 = w.ClearOld();
  EXPECT_EQ(p1.first, 5000);

  // ClearOld 后再 ClearOld 应该返回 0
  std::pair<int64_t, int64_t> p2 = w.ClearOld();
  EXPECT_EQ(p2.first, 0);
  EXPECT_EQ(p2.second, 0);
}

}  // namespace
}  // namespace lalb
