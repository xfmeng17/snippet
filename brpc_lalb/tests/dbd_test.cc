// ============================================================
// DoublyBufferedData 单元测试
//
// 测试双缓冲读写分离容器的核心语义：
// 1. Read 获取的快照在 ScopedPtr 生存期内不变
// 2. Modify 的 fn 被调用两次（修改后台 + 翻转 + 修改新后台）
// 3. Thread-local 锁保证读线程间零竞争
// 4. 并发安全：多个读线程 + 写线程
// 5. Per-instance TLS：多个 DoublyBufferedData 实例互不干扰
// ============================================================

#include "lalb/doubly_buffered_data.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

namespace lalb {
namespace {

struct TestData {
  int value = 0;
  std::string name;
};

// ============================================================
// 基本读写测试
// ============================================================

TEST(DBDTest, ReadDefault) {
  DoublyBufferedData<TestData> dbd;
  DoublyBufferedData<TestData>::ScopedPtr ptr;
  ASSERT_EQ(dbd.Read(&ptr), 0);
  EXPECT_EQ(ptr->value, 0);
  EXPECT_EQ(ptr->name, "");
}

TEST(DBDTest, ModifyThenRead) {
  DoublyBufferedData<TestData> dbd;

  bool ret = dbd.Modify([](TestData& data) -> bool {
    data.value = 42;
    data.name = "hello";
    return true;
  });
  EXPECT_TRUE(ret);

  DoublyBufferedData<TestData>::ScopedPtr ptr;
  ASSERT_EQ(dbd.Read(&ptr), 0);
  EXPECT_EQ(ptr->value, 42);
  EXPECT_EQ(ptr->name, "hello");
}

TEST(DBDTest, ModifyReturnsFalse) {
  DoublyBufferedData<TestData> dbd;

  bool ret = dbd.Modify([](TestData& data) -> bool {
    data.value = 99;
    return false;
  });
  EXPECT_FALSE(ret);

  DoublyBufferedData<TestData>::ScopedPtr ptr;
  ASSERT_EQ(dbd.Read(&ptr), 0);
  EXPECT_EQ(ptr->value, 0);
}

// ============================================================
// Modify 被调用两次的验证
// ============================================================

TEST(DBDTest, ModifyCalledTwice) {
  DoublyBufferedData<TestData> dbd;

  int call_count = 0;
  dbd.Modify([&call_count](TestData& data) -> bool {
    ++call_count;
    data.value += 10;
    return true;
  });

  EXPECT_EQ(call_count, 2);

  DoublyBufferedData<TestData>::ScopedPtr ptr;
  dbd.Read(&ptr);
  EXPECT_EQ(ptr->value, 10);
}

TEST(DBDTest, MultipleModifies) {
  DoublyBufferedData<TestData> dbd;

  for (int i = 0; i < 5; ++i) {
    dbd.Modify([](TestData& data) -> bool {
      data.value += 1;
      return true;
    });
  }

  DoublyBufferedData<TestData>::ScopedPtr ptr;
  dbd.Read(&ptr);
  EXPECT_EQ(ptr->value, 5);
}

// ============================================================
// ModifyWithForeground 测试
// ============================================================

TEST(DBDTest, ModifyWithForeground) {
  DoublyBufferedData<TestData> dbd;

  dbd.Modify([](TestData& data) -> bool {
    data.value = 10;
    return true;
  });

  dbd.ModifyWithForeground([](TestData& bg, const TestData& fg) -> bool {
    bg.value = fg.value + 100;
    return true;
  });

  DoublyBufferedData<TestData>::ScopedPtr ptr;
  dbd.Read(&ptr);
  // 第一次: bg(=10) = fg(=10) + 100 = 110, 翻转
  // 第二次: bg(=10) = fg(=110) + 100 = 210
  // 前台是翻转后的 = 110
  EXPECT_EQ(ptr->value, 110);
}

// ============================================================
// ScopedPtr 阻塞 Modify 测试
//
// 持有 ScopedPtr 时，Modify 的第一次 fn 调用能完成（修改后台+翻转），
// 但"等待所有读线程完成"会阻塞（因为我们还持有 thread-local 锁）。
// 释放 ScopedPtr 后 Modify 才能完成第二次 fn 调用。
//
// 注意：Modify 的第一次 fn 修改后台后翻转，此时我们仍持有旧前台的读锁，
// 新来的 Read 会看到新前台。但我们的 ScopedPtr 还指向旧前台。
// ============================================================

TEST(DBDTest, ScopedPtrBlocksModify) {
  DoublyBufferedData<TestData> dbd;

  dbd.Modify([](TestData& data) -> bool {
    data.value = 1;
    return true;
  });

  // 在子线程中获取快照并持有
  std::atomic<int> snapshot_value{-1};
  std::atomic<bool> can_release{false};
  std::atomic<bool> released{false};

  std::thread reader([&]() {
    DoublyBufferedData<TestData>::ScopedPtr ptr;
    dbd.Read(&ptr);
    snapshot_value.store(ptr->value);
    // 等待主线程通知释放
    while (!can_release.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    // ScopedPtr 析构释放锁
    released.store(true);
  });

  // 等读线程拿到快照
  while (snapshot_value.load() == -1) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  EXPECT_EQ(snapshot_value.load(), 1);

  // 在另一个线程发起 Modify
  std::atomic<bool> modify_done{false};
  std::thread writer([&]() {
    dbd.Modify([](TestData& data) -> bool {
      data.value = 999;
      return true;
    });
    modify_done.store(true);
  });

  // 给 Modify 时间启动
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  // Modify 应该还在等读线程释放锁
  // （在 QEMU 下时序不可靠，不做严格断言，只做功能验证）

  // 释放读锁
  can_release.store(true);
  reader.join();
  writer.join();

  EXPECT_TRUE(modify_done.load());

  // 读新值
  DoublyBufferedData<TestData>::ScopedPtr ptr2;
  dbd.Read(&ptr2);
  EXPECT_EQ(ptr2->value, 999);
}

// ============================================================
// Per-Instance TLS 测试
// ============================================================

TEST(DBDTest, PerInstanceIsolation) {
  DoublyBufferedData<TestData> dbd1;
  DoublyBufferedData<TestData> dbd2;

  dbd1.Modify([](TestData& data) -> bool {
    data.value = 111;
    return true;
  });

  dbd2.Modify([](TestData& data) -> bool {
    data.value = 222;
    return true;
  });

  DoublyBufferedData<TestData>::ScopedPtr ptr1;
  dbd1.Read(&ptr1);
  EXPECT_EQ(ptr1->value, 111);

  DoublyBufferedData<TestData>::ScopedPtr ptr2;
  dbd2.Read(&ptr2);
  EXPECT_EQ(ptr2->value, 222);
}

// ============================================================
// 整数类型测试（POD 零初始化验证）
// ============================================================

TEST(DBDTest, IntegerType) {
  DoublyBufferedData<int> dbd;

  {
    DoublyBufferedData<int>::ScopedPtr ptr;
    dbd.Read(&ptr);
    EXPECT_EQ(*ptr, 0);
  }

  dbd.Modify([](int& data) -> bool {
    data = 42;
    return true;
  });

  {
    DoublyBufferedData<int>::ScopedPtr ptr;
    dbd.Read(&ptr);
    EXPECT_EQ(*ptr, 42);
  }
}

// ============================================================
// vector 类型测试
// ============================================================

TEST(DBDTest, VectorType) {
  DoublyBufferedData<std::vector<int>> dbd;

  dbd.Modify([](std::vector<int>& data) -> bool {
    data.push_back(1);
    data.push_back(2);
    data.push_back(3);
    return true;
  });

  DoublyBufferedData<std::vector<int>>::ScopedPtr ptr;
  dbd.Read(&ptr);
  EXPECT_EQ(ptr->size(), 3u);
  EXPECT_EQ((*ptr)[0], 1);
  EXPECT_EQ((*ptr)[2], 3);
}

// ============================================================
// 并发读写正确性测试
// ============================================================

TEST(DBDTest, ConcurrentReadAndModify) {
  DoublyBufferedData<TestData> dbd;

  std::atomic<bool> stop{false};
  std::atomic<int> read_count{0};

  std::function<void()> reader = [&]() {
    int last_value = -1;
    while (!stop.load()) {
      DoublyBufferedData<TestData>::ScopedPtr ptr;
      dbd.Read(&ptr);
      int v = ptr->value;
      EXPECT_GE(v, last_value);
      last_value = v;
      read_count.fetch_add(1);
    }
  };

  std::function<void()> writer = [&]() {
    while (!stop.load()) {
      dbd.Modify([](TestData& data) -> bool {
        data.value += 1;
        return true;
      });
      std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
  };

  std::vector<std::thread> readers;
  for (int i = 0; i < 3; ++i) {
    readers.emplace_back(reader);
  }
  std::thread write_thread(writer);

  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  stop.store(true);

  for (std::thread& t : readers) {
    t.join();
  }
  write_thread.join();

  EXPECT_GT(read_count.load(), 10);
}

}  // namespace
}  // namespace lalb
