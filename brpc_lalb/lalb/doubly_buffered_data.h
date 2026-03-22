#pragma once

// ============================================================
// DoublyBufferedData: 双缓冲读写分离容器
//
// brpc 中 LALB 的核心并发基础设施。设计目标是让读（Select）几乎无锁，
// 代价是让写（Add/Remove）变得更慢。这在 Load Balancer 场景下是最优的，
// 因为分流请求的频率远高于增删 server 的频率。
//
// ---- 核心设计思想 ----
//
// 1. 双缓冲（Dual Buffer）
//    维护两份数据 _data[0] 和 _data[1]，一份为前台（被所有读线程访问），
//    另一份为后台（仅写线程修改）。通过原子变量 _index 标识哪个是前台。
//
// 2. Thread-Local 锁（而非全局 shared_mutex）
//    每个读线程持有自己的独立 mutex，读操作只锁自己的 mutex。
//    这意味着：
//    - 读线程之间零竞争（各自锁不同的 mutex）
//    - 读操作的开销仅为一次 uncontended pthread_mutex_lock，约 25ns
//    - 对比 shared_mutex：内部有原子 RMW 操作，约 50-100ns + cache bouncing
//
// 3. Modify 流程（写操作）
//    a. 锁 _modify_mutex（串行化所有写操作）
//    b. 修改后台 buffer
//    c. 翻转 _index（memory_order_release），发布新前台
//    d. 遍历所有 thread-local 锁，逐个 lock+unlock
//       → 这保证每个读线程要么已经完成旧前台的读取，
//         要么下次读取时会看到新前台
//    e. 修改新的后台（原来的前台）
//    f. 释放 _modify_mutex
//
//    步骤 d 是关键：lock 一个 thread-local 锁时，如果该线程正在读，
//    我们会等它读完。如果没在读，lock 立即成功（说明它下次读一定看到新 _index）。
//
// 4. 为什么不用 RWLock / shared_mutex？
//    - shared_mutex 内部用原子操作维护读者计数，多核下 cache line bouncing 严重
//    - 读线程越多，shared_mutex 的性能越差（计数器是全局争抢的）
//    - Thread-local 锁完全消除了读线程间的竞争
//    - 代价：写操作需要遍历所有线程的锁（通常只有几十到几百个线程）
//
// 5. Per-Instance TLS 的实现
//    难点：C++ 的 thread_local 是 per-type 的，不是 per-instance 的。
//    如果直接用 `thread_local shared_ptr<Wrapper> w`，同一个类型 T 的
//    所有 DoublyBufferedData<T> 实例会共享同一个 thread_local 变量。
//
//    brpc 的解决方案：WrapperTLSGroup —— 全局的 thread_local 数组池，
//    每个 DoublyBufferedData 实例分配一个唯一 key（类似 pthread_key_create），
//    读取时通过 key 索引到自己的 Wrapper。
//
//    我们的简化方案：thread_local unordered_map<void*, shared_ptr<Wrapper>>，
//    以 this 指针作为 key。每次 Read 多一次 map lookup（O(1) 均摊），
//    比 brpc 的数组索引慢一点，但逻辑清晰。
//
// 6. 对比 brpc 原版
//    - brpc 用 pthread TLS + WrapperTLSGroup（数组池）管理 per-instance Wrapper
//    - 我们用 thread_local unordered_map<this, Wrapper> 实现相同语义
//    - brpc 支持 AllowBthreadSuspended 模式（引用计数替代锁），这里不实现
//    - 保留了核心的 Read/Modify/ModifyWithForeground 三个 API
//
// ---- 使用模式 ----
//
//   DoublyBufferedData<MyData> dbd;
//
//   // 读（高频，几乎无锁）
//   {
//     DoublyBufferedData<MyData>::ScopedPtr ptr;
//     dbd.Read(&ptr);
//     ptr->DoSomething();
//   }  // ScopedPtr 析构时释放 thread-local 锁
//
//   // 写（低频，会等待所有读完成）
//   dbd.Modify([](MyData& data) {
//       data.Add(xxx);
//       return true;
//   });
//
// ============================================================

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace lalb {

template <typename T>
class DoublyBufferedData {
 public:
  // ============================================================
  // Wrapper: 每个线程每个实例一个，持有独立的 mutex
  //
  // 设计要点：
  // - 每个 Wrapper 的 mutex 只会被两方争抢：
  //   (1) 本线程的 Read 操作
  //   (2) 写线程的 Modify 操作（遍历所有 Wrapper 时）
  // - 由于本线程的 Read 不会和自己并发，实际上 mutex 几乎不会被争抢
  // - 只有在 Modify 期间，写线程才会 lock 这个 mutex，等待本线程的 Read 结束
  // ============================================================
  class Wrapper {
   public:
    Wrapper() = default;

    void BeginRead() { mutex_.lock(); }
    void EndRead() { mutex_.unlock(); }

    // 写线程调用：lock + unlock 确保该线程当前没有在读
    void WaitReadDone() {
      std::lock_guard<std::mutex> lock(mutex_);
    }

   private:
    std::mutex mutex_;
  };

  // ============================================================
  // ScopedPtr: RAII 读守卫
  //
  // 构造时锁 thread-local mutex 并获取前台指针，析构时释放锁。
  // 在 ScopedPtr 生存期内，前台数据不会被修改。
  // ============================================================
  class ScopedPtr {
    friend class DoublyBufferedData;

   public:
    ScopedPtr() : data_(nullptr), wrapper_(nullptr) {}

    ~ScopedPtr() {
      if (wrapper_) {
        wrapper_->EndRead();
        wrapper_ = nullptr;
      }
    }

    // 禁止拷贝
    ScopedPtr(const ScopedPtr&) = delete;
    ScopedPtr& operator=(const ScopedPtr&) = delete;

    const T* get() const { return data_; }
    const T& operator*() const { return *data_; }
    const T* operator->() const { return data_; }

   private:
    const T* data_;
    Wrapper* wrapper_;
  };

  DoublyBufferedData();
  ~DoublyBufferedData();  // 清理 TLS 条目

  // 禁止拷贝/移动
  DoublyBufferedData(const DoublyBufferedData&) = delete;
  DoublyBufferedData& operator=(const DoublyBufferedData&) = delete;

  // ============================================================
  // Read: 获取前台数据的只读快照
  //
  // 流程：
  // 1. 获取本线程对应本实例的 Wrapper（首次调用时创建并注册）
  // 2. 锁 Wrapper 的 mutex（本线程独占，几乎不会争抢）
  // 3. 读 _index（memory_order_acquire），获取前台 buffer 指针
  // 4. 返回 0 表示成功
  //
  // ScopedPtr 析构时自动释放锁。
  // ============================================================
  int Read(ScopedPtr* ptr);

  // ============================================================
  // Modify: 修改双缓冲数据
  //
  // fn 签名: bool fn(T& background_data)
  // fn 会被调用两次：一次修改后台，一次修改新后台（翻转后的原前台）
  //
  // 返回 fn 第一次的返回值
  // ============================================================
  bool Modify(std::function<bool(T&)> fn);

  // ============================================================
  // ModifyWithForeground: 修改时可以参考前台数据
  //
  // fn 签名: bool fn(T& background, const T& foreground)
  //
  // 用途：AddServer 时，第一次调用创建 Weight，第二次从 fg 复制指针。
  // ============================================================
  bool ModifyWithForeground(std::function<bool(T& bg, const T& fg)> fn);

  // 获取当前活跃 wrapper 数量（调试用）
  size_t WrapperCount() const;

 private:
  const T* UnsafeRead() const {
    return &data_[index_.load(std::memory_order_acquire)];
  }

  // 获取或创建本线程对应本实例的 Wrapper
  Wrapper* GetOrCreateWrapper();

  // ---- Per-Instance TLS ----
  // 每个线程维护一个 map: DoublyBufferedData* → Wrapper
  // 这样不同的 DoublyBufferedData 实例各自有独立的 Wrapper
  struct TlsMap {
    std::unordered_map<const void*, std::shared_ptr<Wrapper>> wrappers;
  };
  static TlsMap& GetTlsMap() {
    thread_local TlsMap tls_map;
    return tls_map;
  }

  // ---- 数据成员 ----

  // 双缓冲
  T data_[2]{};  // 值初始化，对 POD 类型零初始化

  // 前台 buffer 索引
  std::atomic<int> index_{0};

  // 所有线程的 Wrapper 弱引用
  std::vector<std::weak_ptr<Wrapper>> wrappers_;

  // 保护 _wrappers 的锁
  std::mutex wrappers_mutex_;

  // 串行化所有 Modify 操作
  std::mutex modify_mutex_;
};

// ============================================================
// 实现
// ============================================================

template <typename T>
DoublyBufferedData<T>::DoublyBufferedData() {
  wrappers_.reserve(64);
}

template <typename T>
DoublyBufferedData<T>::~DoublyBufferedData() {
  // 清理当前线程的 TLS 条目，避免地址复用时引用到旧 Wrapper。
  // 注意：其他线程的 TLS 条目在线程退出时由 shared_ptr 析构自动清理。
  // 这里只清理当前线程（通常是创建者线程）的条目。
  TlsMap& tls = GetTlsMap();
  tls.wrappers.erase(this);

  // 标记所有 Wrapper 为脱离状态（避免野指针）
  std::lock_guard<std::mutex> lock(wrappers_mutex_);
  wrappers_.clear();
}

template <typename T>
typename DoublyBufferedData<T>::Wrapper*
DoublyBufferedData<T>::GetOrCreateWrapper() {
  // ---- Per-Instance TLS 核心实现 ----
  //
  // brpc 用 WrapperTLSGroup：一个全局的 thread_local 数组池。
  // 每个 DoublyBufferedData 实例分配一个唯一 ID（类似 pthread_key_create），
  // 通过 ID 索引数组得到对应的 Wrapper。
  //
  // 我们用更简单的方案：thread_local unordered_map，以 this 为 key。
  // 每次 Read 多一次 hash lookup（O(1)），换来更简单的代码。
  // 在 Load Balancer 场景下，一个进程通常只有个位数的 DoublyBufferedData 实例，
  // 所以 map 很小，性能差异可以忽略。
  TlsMap& tls = GetTlsMap();
  std::shared_ptr<Wrapper>& w = tls.wrappers[this];
  if (!w) {
    w = std::make_shared<Wrapper>();
    std::lock_guard<std::mutex> lock(wrappers_mutex_);
    wrappers_.push_back(w);
  }
  return w.get();
}

template <typename T>
int DoublyBufferedData<T>::Read(ScopedPtr* ptr) {
  // 如果 ptr 已经持有旧锁，先释放
  if (ptr->wrapper_) {
    ptr->wrapper_->EndRead();
    ptr->wrapper_ = nullptr;
    ptr->data_ = nullptr;
  }

  Wrapper* w = GetOrCreateWrapper();
  if (!w) {
    return -1;
  }
  w->BeginRead();
  ptr->data_ = UnsafeRead();
  ptr->wrapper_ = w;
  return 0;
}

template <typename T>
bool DoublyBufferedData<T>::Modify(std::function<bool(T&)> fn) {
  std::lock_guard<std::mutex> modify_lock(modify_mutex_);

  int bg_index = !index_.load(std::memory_order_relaxed);

  // 第一次修改：后台 buffer
  bool ret = fn(data_[bg_index]);
  if (!ret) {
    return false;
  }

  // 翻转前后台
  index_.store(bg_index, std::memory_order_release);
  bg_index = !bg_index;

  // 等待所有读线程完成对旧前台的访问
  {
    std::lock_guard<std::mutex> lock(wrappers_mutex_);
    size_t alive = 0;
    for (size_t i = 0; i < wrappers_.size(); ++i) {
      std::shared_ptr<Wrapper> w = wrappers_[i].lock();
      if (w) {
        w->WaitReadDone();
        wrappers_[alive] = wrappers_[i];
        ++alive;
      }
    }
    wrappers_.resize(alive);
  }

  // 第二次修改：新后台（原前台）
  fn(data_[bg_index]);
  return true;
}

template <typename T>
bool DoublyBufferedData<T>::ModifyWithForeground(
    std::function<bool(T& bg, const T& fg)> fn) {
  return Modify([this, &fn](T& bg) -> bool {
    const T& fg = data_[(&bg == &data_[0]) ? 1 : 0];
    return fn(bg, fg);
  });
}

template <typename T>
size_t DoublyBufferedData<T>::WrapperCount() const {
  std::lock_guard<std::mutex> lock(
      const_cast<std::mutex&>(wrappers_mutex_));
  size_t count = 0;
  for (size_t i = 0; i < wrappers_.size(); ++i) {
    if (!wrappers_[i].expired()) {
      ++count;
    }
  }
  return count;
}

}  // namespace lalb
