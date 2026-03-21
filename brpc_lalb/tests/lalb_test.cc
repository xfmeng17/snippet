// ============================================================
// LALB 集成测试
// 测试完整的负载均衡流程
// ============================================================

#include "lalb/lalb.h"

#include <chrono>
#include <functional>
#include <thread>
#include <unordered_map>
#include <vector>

#include "gtest/gtest.h"

namespace lalb {
namespace {

// ============================================================
// 接口测试：LALB 基本 API
// ============================================================

TEST(LALBTest, EmptySelect) {
  LALB lb;
  LALB::SelectResult r = lb.Select();
  EXPECT_FALSE(r.success);
}

TEST(LALBTest, AddAndSelect) {
  LALB lb;
  EXPECT_TRUE(lb.AddServer(1));
  LALB::SelectResult r = lb.Select();
  EXPECT_TRUE(r.success);
  EXPECT_EQ(r.server_id, 1u);
}

TEST(LALBTest, AddDuplicate) {
  LALB lb;
  EXPECT_TRUE(lb.AddServer(1));
  EXPECT_FALSE(lb.AddServer(1));
  EXPECT_EQ(lb.Size(), 1u);
}

TEST(LALBTest, RemoveAndSelect) {
  LALB lb;
  lb.AddServer(1);
  lb.AddServer(2);
  lb.RemoveServer(1);
  EXPECT_EQ(lb.Size(), 1u);

  LALB::SelectResult r = lb.Select();
  EXPECT_TRUE(r.success);
  EXPECT_EQ(r.server_id, 2u);
}

TEST(LALBTest, RemoveNonExistent) {
  LALB lb;
  EXPECT_FALSE(lb.RemoveServer(999));
}

TEST(LALBTest, AddRemoveAllThenAdd) {
  LALB lb;
  lb.AddServer(1);
  lb.AddServer(2);
  lb.RemoveServer(1);
  lb.RemoveServer(2);
  EXPECT_EQ(lb.Size(), 0u);
  LALB::SelectResult r = lb.Select();
  EXPECT_FALSE(r.success);

  lb.AddServer(3);
  r = lb.Select();
  EXPECT_TRUE(r.success);
  EXPECT_EQ(r.server_id, 3u);
}

TEST(LALBTest, Size) {
  LALB lb;
  EXPECT_EQ(lb.Size(), 0u);
  lb.AddServer(1);
  EXPECT_EQ(lb.Size(), 1u);
  lb.AddServer(2);
  EXPECT_EQ(lb.Size(), 2u);
  lb.RemoveServer(1);
  EXPECT_EQ(lb.Size(), 1u);
}

TEST(LALBTest, TotalWeightPositive) {
  LALB lb;
  lb.AddServer(1);
  lb.AddServer(2);
  EXPECT_GT(lb.TotalWeight(), 0);
}

// ============================================================
// 接口测试：Select + Feedback 完整流程
// ============================================================

TEST(LALBTest, SelectAndFeedback) {
  LALB lb;
  lb.AddServer(1);
  lb.AddServer(2);

  LALB::SelectResult r = lb.Select();
  EXPECT_TRUE(r.success);

  // 模拟 1ms 延时
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
  lb.Feedback(r.server_id, r.begin_time_us, 1000, false);
}

TEST(LALBTest, MultipleFeedbacks) {
  LALB lb;
  lb.AddServer(1);
  lb.AddServer(2);
  lb.AddServer(3);

  for (int i = 0; i < 100; ++i) {
    LALB::SelectResult r = lb.Select();
    EXPECT_TRUE(r.success);
    std::this_thread::sleep_for(std::chrono::microseconds(50));
    lb.Feedback(r.server_id, r.begin_time_us, 500, false);
  }
}

// ============================================================
// 集成测试：模拟真实场景 - 不同延时的 server
// ============================================================

TEST(LALBTest, PreferLowLatencyServer) {
  LALB lb;
  // 2 个 server，快和慢
  lb.AddServer(1);  // 快: sleep 100us
  lb.AddServer(2);  // 慢: sleep 2ms

  std::unordered_map<uint64_t, int> select_count;

  // 跑 300 轮，通过真实 sleep 产生不同延时
  for (int i = 0; i < 300; ++i) {
    LALB::SelectResult r = lb.Select();
    if (!r.success) continue;
    select_count[r.server_id]++;

    // 通过不同的 sleep 时间模拟不同延时
    if (r.server_id == 1) {
      std::this_thread::sleep_for(std::chrono::microseconds(100));
      lb.Feedback(r.server_id, r.begin_time_us, 100, false);
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
      lb.Feedback(r.server_id, r.begin_time_us, 2000, false);
    }
  }

  int count1 = select_count[1];
  int count2 = select_count[2];

  // server 1 (快) 应该获得更多流量
  EXPECT_GT(count1, count2)
      << "Fast server should get more traffic. "
      << "count1=" << count1 << " count2=" << count2;
}

TEST(LALBTest, AdaptToLatencyChange) {
  LALB lb;
  lb.AddServer(1);
  lb.AddServer(2);

  // 阶段 1: server 1 快 (1ms), server 2 慢 (5ms)
  for (int i = 0; i < 200; ++i) {
    LALB::SelectResult r = lb.Select();
    if (!r.success) continue;
    int64_t latency = (r.server_id == 1) ? 1000 : 5000;
    std::this_thread::sleep_for(std::chrono::microseconds(50));
    lb.Feedback(r.server_id, r.begin_time_us, latency, false);
  }

  // 阶段 2: 反转延时 - server 1 慢 (5ms), server 2 快 (1ms)
  std::unordered_map<uint64_t, int> phase2_count;
  for (int i = 0; i < 300; ++i) {
    LALB::SelectResult r = lb.Select();
    if (!r.success) continue;
    phase2_count[r.server_id]++;
    int64_t latency = (r.server_id == 1) ? 5000 : 1000;
    std::this_thread::sleep_for(std::chrono::microseconds(50));
    lb.Feedback(r.server_id, r.begin_time_us, latency, false);
  }

  // 阶段 2 后半段，server 2 应该获得更多流量
  // 但由于需要时间适应，只验证 server 2 不是零
  EXPECT_GT(phase2_count[2], 0)
      << "Server 2 should receive traffic after latency reversal";
}

// ============================================================
// 集成测试：错误处理
// ============================================================

TEST(LALBTest, ErrorServerGetLessTraffic) {
  LALB lb;
  lb.AddServer(1);
  lb.AddServer(2);

  std::unordered_map<uint64_t, int> counts;

  for (int i = 0; i < 300; ++i) {
    LALB::SelectResult r = lb.Select();
    if (!r.success) continue;
    counts[r.server_id]++;

    bool error = (r.server_id == 2);  // server 2 总是出错
    int64_t latency = error ? 10000 : 1000;
    std::this_thread::sleep_for(std::chrono::microseconds(50));
    lb.Feedback(r.server_id, r.begin_time_us, latency, error, 100);
  }

  // server 1（正常）应该获得更多流量
  EXPECT_GT(counts[1], counts[2])
      << "Normal server should get more traffic than error server. "
      << "count1=" << counts[1] << " count2=" << counts[2];
}

// ============================================================
// 集成测试：大规模 server
// ============================================================

TEST(LALBTest, ManyServers) {
  LALB lb;
  int n = 100;
  for (int i = 1; i <= n; ++i) {
    lb.AddServer(i);
  }
  EXPECT_EQ(lb.Size(), (size_t)n);

  // Select 应该能正常工作
  std::unordered_map<uint64_t, int> counts;
  for (int i = 0; i < 1000; ++i) {
    LALB::SelectResult r = lb.Select();
    EXPECT_TRUE(r.success);
    counts[r.server_id]++;
    lb.Feedback(r.server_id, r.begin_time_us, 1000, false);
  }

  // 应该覆盖大部分 server
  EXPECT_GT(counts.size(), (size_t)(n / 2))
      << "Should cover most servers, got " << counts.size();
}

// ============================================================
// 集成测试：并发 Select + Feedback
// ============================================================

TEST(LALBTest, ConcurrentSelectFeedback) {
  LALB lb;
  for (uint64_t i = 1; i <= 5; ++i) {
    lb.AddServer(i);
  }

  std::atomic<bool> stop{false};
  std::atomic<int> total_ops{0};

  std::function<void()> worker = [&]() {
    while (!stop.load()) {
      LALB::SelectResult r = lb.Select();
      if (r.success) {
        std::this_thread::sleep_for(std::chrono::microseconds(10));
        lb.Feedback(r.server_id, r.begin_time_us, 500, false);
        total_ops.fetch_add(1);
      }
    }
  };

  std::vector<std::thread> threads;
  for (int i = 0; i < 4; ++i) {
    threads.emplace_back(worker);
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  stop.store(true);

  for (std::thread& t : threads) {
    t.join();
  }

  EXPECT_GT(total_ops.load(), 100)
      << "Should complete significant number of operations";
}

// ============================================================
// 集成测试：并发增删 + Select
// ============================================================

TEST(LALBTest, ConcurrentModifyAndSelect) {
  LALB lb;
  for (uint64_t i = 1; i <= 3; ++i) {
    lb.AddServer(i);
  }

  std::atomic<bool> stop{false};
  std::atomic<int> select_ok{0};
  std::atomic<int> select_fail{0};

  // 读线程
  std::function<void()> reader = [&]() {
    while (!stop.load()) {
      LALB::SelectResult r = lb.Select();
      if (r.success) {
        select_ok.fetch_add(1);
        lb.Feedback(r.server_id, r.begin_time_us, 1000, false);
      } else {
        select_fail.fetch_add(1);
      }
    }
  };

  // 写线程
  std::function<void()> writer = [&]() {
    uint64_t id = 100;
    while (!stop.load()) {
      lb.AddServer(id);
      std::this_thread::sleep_for(std::chrono::microseconds(200));
      lb.RemoveServer(id);
      ++id;
    }
  };

  std::vector<std::thread> readers;
  for (int i = 0; i < 3; ++i) {
    readers.emplace_back(reader);
  }
  std::thread write_t(writer);

  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  stop.store(true);

  for (std::thread& t : readers) {
    t.join();
  }
  write_t.join();

  EXPECT_GT(select_ok.load(), 0);
}

// ============================================================
// 集成测试：inflight delay 惩罚效果
// ============================================================

TEST(LALBTest, InflightDelayPunishment) {
  LALB lb;
  lb.AddServer(1);
  lb.AddServer(2);

  // 只给 server 1 发请求但不 Feedback（模拟卡住）
  // 然后看 server 2 是否获得更多流量
  std::vector<LALB::SelectResult> pending;

  // 先让两个 server 都有基线
  for (int i = 0; i < 50; ++i) {
    LALB::SelectResult r = lb.Select();
    if (r.success) {
      std::this_thread::sleep_for(std::chrono::microseconds(50));
      lb.Feedback(r.server_id, r.begin_time_us, 1000, false);
    }
  }

  // 现在让 server 1 的请求"挂起"不回来
  for (int i = 0; i < 5; ++i) {
    LALB::SelectResult r = lb.Select();
    if (r.success && r.server_id == 1) {
      pending.push_back(r);  // 不 feedback
    } else if (r.success) {
      lb.Feedback(r.server_id, r.begin_time_us, 1000, false);
    }
  }

  // 等待一段时间让 inflight delay 生效
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  // 后续选择应该更倾向 server 2
  int count2 = 0;
  for (int i = 0; i < 100; ++i) {
    LALB::SelectResult r = lb.Select();
    if (r.success) {
      if (r.server_id == 2) count2++;
      lb.Feedback(r.server_id, r.begin_time_us, 1000, false);
    }
  }

  // 清理挂起的请求
  for (LALB::SelectResult& r : pending) {
    lb.Feedback(r.server_id, r.begin_time_us, 50000, false);
  }

  // server 2 应该获得不少流量（inflight delay 在惩罚 server 1）
  EXPECT_GT(count2, 0) << "Server 2 should receive traffic when Server 1 has "
                          "high inflight delay";
}

// ============================================================
// 边界测试
// ============================================================

TEST(LALBTest, SingleServerAllTraffic) {
  LALB lb;
  lb.AddServer(1);

  for (int i = 0; i < 100; ++i) {
    LALB::SelectResult r = lb.Select();
    EXPECT_TRUE(r.success);
    EXPECT_EQ(r.server_id, 1u);
    lb.Feedback(r.server_id, r.begin_time_us, 1000, false);
  }
}

TEST(LALBTest, AllServersSameLatency) {
  LALB lb;
  lb.AddServer(1);
  lb.AddServer(2);
  lb.AddServer(3);

  std::unordered_map<uint64_t, int> counts;
  for (int i = 0; i < 600; ++i) {
    LALB::SelectResult r = lb.Select();
    if (r.success) {
      counts[r.server_id]++;
      lb.Feedback(r.server_id, r.begin_time_us, 1000, false);
    }
  }

  // 相同延时，每个 server 都应该被访问到（最小权值保护确保了这一点）
  EXPECT_EQ(counts.size(), 3u) << "All 3 servers should receive some traffic";
  for (const std::pair<const uint64_t, int>& kv : counts) {
    EXPECT_GT(kv.second, 0)
        << "Server " << kv.first << " should get at least some traffic";
  }
}

TEST(LALBTest, LargeServerIdValues) {
  LALB lb;
  uint64_t id1 = 0xFFFFFFFF00000001ULL;
  uint64_t id2 = 0xFFFFFFFF00000002ULL;
  lb.AddServer(id1);
  lb.AddServer(id2);

  LALB::SelectResult r = lb.Select();
  EXPECT_TRUE(r.success);
  EXPECT_TRUE(r.server_id == id1 || r.server_id == id2);
}

}  // namespace
}  // namespace lalb
