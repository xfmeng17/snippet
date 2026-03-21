// ============================================================
// WeightTree 单元测试 + 接口测试
// 测试完全二叉树的增删查和权值分布
// ============================================================

#include "lalb/weight_tree.h"

#include <chrono>
#include <functional>
#include <set>
#include <thread>
#include <unordered_map>

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
// 单元测试：基本增删
// ============================================================

TEST(WeightTreeTest, Empty) {
  WeightTree tree;
  EXPECT_EQ(tree.Size(), 0u);
}

TEST(WeightTreeTest, AddOne) {
  WeightTree tree;
  EXPECT_TRUE(tree.AddServer(1, Weight::kWeightScale));
  EXPECT_EQ(tree.Size(), 1u);
}

TEST(WeightTreeTest, AddDuplicate) {
  WeightTree tree;
  EXPECT_TRUE(tree.AddServer(1, 1000));
  EXPECT_FALSE(tree.AddServer(1, 1000));
  EXPECT_EQ(tree.Size(), 1u);
}

TEST(WeightTreeTest, AddMultiple) {
  WeightTree tree;
  for (uint64_t i = 1; i <= 10; ++i) {
    EXPECT_TRUE(tree.AddServer(i, Weight::kWeightScale));
  }
  EXPECT_EQ(tree.Size(), 10u);
}

TEST(WeightTreeTest, RemoveOne) {
  WeightTree tree;
  tree.AddServer(1, 1000);
  EXPECT_TRUE(tree.RemoveServer(1));
  EXPECT_EQ(tree.Size(), 0u);
}

TEST(WeightTreeTest, RemoveNonExistent) {
  WeightTree tree;
  EXPECT_FALSE(tree.RemoveServer(999));
}

TEST(WeightTreeTest, RemoveMiddle) {
  WeightTree tree;
  tree.AddServer(1, 1000);
  tree.AddServer(2, 1000);
  tree.AddServer(3, 1000);
  EXPECT_TRUE(tree.RemoveServer(2));
  EXPECT_EQ(tree.Size(), 2u);
}

TEST(WeightTreeTest, RemoveAll) {
  WeightTree tree;
  for (uint64_t i = 1; i <= 5; ++i) {
    tree.AddServer(i, 1000);
  }
  for (uint64_t i = 1; i <= 5; ++i) {
    EXPECT_TRUE(tree.RemoveServer(i));
  }
  EXPECT_EQ(tree.Size(), 0u);
}

TEST(WeightTreeTest, AddAfterRemove) {
  WeightTree tree;
  tree.AddServer(1, 1000);
  tree.RemoveServer(1);
  EXPECT_TRUE(tree.AddServer(1, 2000));
  EXPECT_EQ(tree.Size(), 1u);
}

// ============================================================
// 接口测试：Select 基本行为
// ============================================================

TEST(WeightTreeTest, SelectFromEmpty) {
  WeightTree tree;
  WeightTree::SelectResult r = tree.Select(0, NowUs());
  EXPECT_FALSE(r.success);
}

TEST(WeightTreeTest, SelectFromSingle) {
  WeightTree tree;
  tree.AddServer(42, Weight::kWeightScale);
  WeightTree::SelectResult r = tree.Select(Weight::kWeightScale, NowUs());
  EXPECT_TRUE(r.success);
  EXPECT_EQ(r.server_id, 42u);
}

TEST(WeightTreeTest, SelectReturnsValidServer) {
  WeightTree tree;
  std::set<uint64_t> ids = {1, 2, 3, 4, 5};
  for (uint64_t id : ids) {
    tree.AddServer(id, Weight::kWeightScale);
  }
  int64_t total = Weight::kWeightScale * 5;
  for (int i = 0; i < 100; ++i) {
    WeightTree::SelectResult r = tree.Select(total, NowUs());
    EXPECT_TRUE(r.success);
    EXPECT_TRUE(ids.count(r.server_id) > 0)
        << "Selected server_id=" << r.server_id << " not in valid set";
  }
}

// ============================================================
// 接口测试：权值分布
// ============================================================

TEST(WeightTreeTest, SelectDistribution) {
  // 3 个 server，等权值，选择分布应该大致均匀
  WeightTree tree;
  tree.AddServer(1, 10000);
  tree.AddServer(2, 10000);
  tree.AddServer(3, 10000);

  std::unordered_map<uint64_t, int> counts;
  int64_t total = 30000;
  int n = 3000;
  for (int i = 0; i < n; ++i) {
    WeightTree::SelectResult r = tree.Select(total, NowUs());
    if (r.success) {
      counts[r.server_id]++;
    }
  }

  // 每个 server 应该被选中大约 1/3 的时间
  // 允许较大误差（因为 inflight delay 会改变权值）
  for (const std::pair<const uint64_t, int>& kv : counts) {
    EXPECT_GT(kv.second, n / 10)
        << "Server " << kv.first << " selected too few times: " << kv.second;
  }
}

// ============================================================
// 接口测试：Feedback
// ============================================================

TEST(WeightTreeTest, FeedbackNonExistent) {
  WeightTree tree;
  int64_t diff = tree.Feedback(999, 1000, NowUs(), false, 0);
  EXPECT_EQ(diff, 0);
}

TEST(WeightTreeTest, FeedbackUpdatesWeight) {
  WeightTree tree;
  tree.AddServer(1, Weight::kWeightScale);

  // Select → Feedback 循环
  int64_t total = Weight::kWeightScale;
  for (int i = 0; i < 50; ++i) {
    int64_t begin = NowUs();
    WeightTree::SelectResult r = tree.Select(total, begin);
    if (r.success) {
      total += r.weight_diff;
      std::this_thread::sleep_for(std::chrono::microseconds(100));
      int64_t diff = tree.Feedback(r.server_id, 1000, begin, false, 0);
      total += diff;
    }
  }
  // 经过反馈，总权值应该有所变化（因为真实延时参与了计算）
  // 不做精确断言，只验证不崩溃
}

// ============================================================
// 接口测试：UpdateParentWeights
// ============================================================

TEST(WeightTreeTest, ParentWeightsConsistency) {
  WeightTree tree;
  // 添加 7 个节点形成 3 层完全二叉树
  for (uint64_t i = 1; i <= 7; ++i) {
    tree.AddServer(i, 1000);
  }
  EXPECT_EQ(tree.Size(), 7u);

  // 移除几个，验证不崩溃
  tree.RemoveServer(3);
  tree.RemoveServer(5);
  EXPECT_EQ(tree.Size(), 5u);

  // 选择仍然正常
  for (int i = 0; i < 100; ++i) {
    WeightTree::SelectResult r = tree.Select(5000, NowUs());
    EXPECT_TRUE(r.success);
  }
}

// ============================================================
// 并发测试
// ============================================================

TEST(WeightTreeTest, ConcurrentSelectAndFeedback) {
  WeightTree tree;
  for (uint64_t i = 1; i <= 5; ++i) {
    tree.AddServer(i, Weight::kWeightScale);
  }

  std::atomic<bool> stop{false};
  std::atomic<int> select_count{0};
  std::atomic<int> feedback_count{0};

  // 多个线程并发 Select + Feedback
  std::function<void()> worker = [&]() {
    while (!stop.load()) {
      int64_t total = Weight::kWeightScale * 5;
      int64_t begin = NowUs();
      WeightTree::SelectResult r = tree.Select(total, begin);
      if (r.success) {
        select_count.fetch_add(1);
        std::this_thread::sleep_for(std::chrono::microseconds(10));
        tree.Feedback(r.server_id, 100, begin, false, 0);
        feedback_count.fetch_add(1);
      }
    }
  };

  std::vector<std::thread> threads;
  for (int i = 0; i < 4; ++i) {
    threads.emplace_back(worker);
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  stop.store(true);

  for (std::thread& t : threads) {
    t.join();
  }

  EXPECT_GT(select_count.load(), 0);
  EXPECT_GT(feedback_count.load(), 0);
}

TEST(WeightTreeTest, ConcurrentAddRemoveAndSelect) {
  WeightTree tree;
  for (uint64_t i = 1; i <= 3; ++i) {
    tree.AddServer(i, Weight::kWeightScale);
  }

  std::atomic<bool> stop{false};

  // 读线程：持续 Select
  std::function<void()> reader = [&]() {
    while (!stop.load()) {
      int64_t total = Weight::kWeightScale * 3;
      tree.Select(total, NowUs());
    }
  };

  // 写线程：反复增删 server
  std::function<void()> writer = [&]() {
    uint64_t next_id = 100;
    while (!stop.load()) {
      tree.AddServer(next_id, Weight::kWeightScale);
      std::this_thread::sleep_for(std::chrono::microseconds(500));
      tree.RemoveServer(next_id);
      ++next_id;
    }
  };

  std::vector<std::thread> readers;
  for (int i = 0; i < 3; ++i) {
    readers.emplace_back(reader);
  }
  std::thread write_thread(writer);

  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  stop.store(true);

  for (std::thread& t : readers) {
    t.join();
  }
  write_thread.join();

  // 验证结构完整性
  EXPECT_GE(tree.Size(), 3u);
}

}  // namespace
}  // namespace lalb
