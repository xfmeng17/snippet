# brpc LALB 学习项目

从 [brpc](https://github.com/apache/brpc) 中提取 Locality-Aware Load Balancing 算法，
重写为可读性好的独立 C++ 实现，用 Bazel 构建。

## 目标

1. **理解** LALB 算法的核心思想和工程实现
2. **重写** 一个简化但完整的版本，去掉 brpc 框架依赖
3. **学习** 高并发场景下的负载均衡设计

## 项目结构

```
brpc_lalb/
├── TASK.md                    # 本文件
├── MODULE.bazel               # Bazel module 定义
├── BUILD.bazel                # 顶层 build
├── docs/
│   ├── algorithm.md           # 算法讲解（课程式）
│   └── architecture.md        # 架构对照图
├── lalb/
│   ├── BUILD.bazel
│   ├── weight.h               # 权值计算（base_weight + inflight delay）
│   ├── weight.cc
│   ├── weight_tree.h          # 完全二叉树 O(logN) 查找
│   ├── weight_tree.cc
│   ├── doubly_buffered_data.h # 读写分离的双缓冲容器
│   ├── lalb.h                 # 负载均衡器主类
│   └── lalb.cc
└── tests/
    ├── BUILD.bazel
    ├── weight_test.cc
    ├── weight_tree_test.cc
    └── lalb_test.cc
```

## 原项目文件对照

| 本项目 | brpc 原文件 |
|--------|------------|
| `lalb/doubly_buffered_data.h` | `butil/containers/doubly_buffered_data.h` |
| `lalb/weight.h/cc` | `Weight` 内部类 in `locality_aware_load_balancer.h/cpp` |
| `lalb/weight_tree.h/cc` | `Servers` 内部类 + `SelectServer()` |
| `lalb/lalb.h/cc` | `LocalityAwareLoadBalancer` 主类 |
