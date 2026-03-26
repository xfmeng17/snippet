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
#include <utility>
#include <vector>

#include "gtest/gtest.h"

namespace lalb {
namespace {

static int64_t NowUs() {
  std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
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
  EXPECT_GT(tree.AddServer(1), 0);
  EXPECT_EQ(tree.Size(), 1u);
}

TEST(WeightTreeTest, AddDuplicate) {
  WeightTree tree;
  EXPECT_GT(tree.AddServer(1), 0);
  EXPECT_EQ(tree.AddServer(1), 0);
  EXPECT_EQ(tree.Size(), 1u);
}

TEST(WeightTreeTest, AddMultiple) {
  WeightTree tree;
  for (uint64_t i = 1; i <= 10; ++i) {
    EXPECT_GT(tree.AddServer(i), 0);
  }
  EXPECT_EQ(tree.Size(), 10u);
}

TEST(WeightTreeTest, RemoveOne) {
  WeightTree tree;
  tree.AddServer(1);
  EXPECT_GT(tree.RemoveServer(1), 0);
  EXPECT_EQ(tree.Size(), 0u);
}

TEST(WeightTreeTest, RemoveNonExistent) {
  WeightTree tree;
  EXPECT_EQ(tree.RemoveServer(999), 0);
}

TEST(WeightTreeTest, RemoveMiddle) {
  WeightTree tree;
  tree.AddServer(1);
  tree.AddServer(2);
  tree.AddServer(3);
  EXPECT_GT(tree.RemoveServer(2), 0);
  EXPECT_EQ(tree.Size(), 2u);
}

TEST(WeightTreeTest, RemoveAll) {
  WeightTree tree;
  for (uint64_t i = 1; i <= 5; ++i) {
    tree.AddServer(i);
  }
  for (uint64_t i = 1; i <= 5; ++i) {
    EXPECT_GT(tree.RemoveServer(i), 0);
  }
  EXPECT_EQ(tree.Size(), 0u);
}

TEST(WeightTreeTest, AddAfterRemove) {
  WeightTree tree;
  tree.AddServer(1);
  tree.RemoveServer(1);
  EXPECT_GT(tree.AddServer(1), 0);
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
  int64_t w = tree.AddServer(42);
  WeightTree::SelectResult r = tree.Select(w, NowUs());
  EXPECT_TRUE(r.success);
  EXPECT_EQ(r.server_id, 42u);
}

TEST(WeightTreeTest, SelectReturnsValidServer) {
  WeightTree tree;
  std::set<uint64_t> ids = {1, 2, 3, 4, 5};
  int64_t total = 0;
  for (uint64_t id : ids) {
    total += tree.AddServer(id);
  }
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
  WeightTree tree;
  int64_t total = 0;
  total += tree.AddServer(1);
  total += tree.AddServer(2);
  total += tree.AddServer(3);

  std::unordered_map<uint64_t, int> counts;
  int n = 3000;
  for (int i = 0; i < n; ++i) {
    WeightTree::SelectResult r = tree.Select(total, NowUs());
    if (r.success) {
      counts[r.server_id]++;
    }
  }

  for (const std::pair<const uint64_t, int>& kv : counts) {
    EXPECT_GT(kv.second, n / 10) << "Server " << kv.first
                                 << " selected too few times: " << kv.second;
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
  int64_t total = tree.AddServer(1);
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
}

// ============================================================
// 接口测试：UpdateParentWeights 一致性
// ============================================================

TEST(WeightTreeTest, ParentWeightsConsistency) {
  WeightTree tree;
  for (uint64_t i = 1; i <= 7; ++i) {
    tree.AddServer(i);
  }
  EXPECT_EQ(tree.Size(), 7u);

  tree.RemoveServer(3);
  tree.RemoveServer(5);
  EXPECT_EQ(tree.Size(), 5u);

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
    tree.AddServer(i);
  }

  std::atomic<bool> stop{false};
  std::atomic<int> select_count{0};
  std::atomic<int> feedback_count{0};

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
    tree.AddServer(i);
  }

  std::atomic<bool> stop{false};

  std::function<void()> reader = [&]() {
    while (!stop.load()) {
      int64_t total = Weight::kWeightScale * 3;
      tree.Select(total, NowUs());
    }
  };

  std::function<void()> writer = [&]() {
    uint64_t next_id = 100;
    while (!stop.load()) {
      tree.AddServer(next_id);
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

  EXPECT_GE(tree.Size(), 3u);
}

}  // namespace
}  // namespace lalb
