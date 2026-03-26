# snippet

个人技术学习仓库：代码片段 + 技术调研文档。

代码片段不是简化版，不是玩具 demo——目标是**同等完整度下更可读、代码规范更清晰的重写**。读代码就能学会原库的核心工程决策。

## 学习方法

1. **选题**：从知名 C++ 项目中选出值得深入理解的核心模块
2. **重写**：用纯 STL 重写，去掉框架依赖，保留完整算法逻辑
3. **文档**：每个模块配数值走读文档（具体数值演示，可手算验证）
4. **测试**：充分的单元测试 + 集成测试 + 并发测试

## 项目列表

| 目录 | 类型 | 主题 | 说明 |
|------|------|------|------|
| [brpc_lalb](brpc_lalb/) | 代码重写 | Locality-Aware Load Balancing | 源自 [Apache brpc](https://github.com/apache/brpc)，Bazel, C++17 |
| [concurrent_hashmap](concurrent_hashmap/) | 技术调研 | C++ 并发 HashMap | 业界主流方案对比：读 wait-free + 写分片锁 到 完全 lock-free |
| [zenmux](zenmux/) | 技术调研 | ZenMux LLM API 代理商 | 收费方式详解（订阅 Flow 机制、PAYG、Claude API 计费对比） |
| [ccuse](ccuse/) | 工具 | Claude Code Provider 切换器 | 30 行 shell 函数，零依赖秒切 provider，替代 cc-switch |
| [claude](claude/) | 工具 | Claude Code 自定义 Skills | learn-project、cpp-style-check、dev-init、gcp 等自定义技能 |

## brpc_lalb

从 brpc 中提取 LALB（Locality-Aware Load Balancing）算法，完整重写。

核心学习点：

- **DoublyBufferedData**：读写分离的双缓冲容器，读路径近乎无锁（thread-local mutex）
- **WeightTree**：完全二叉树按权值 O(logN) 随机选择，原子操作更新 left_weight
- **Weight**：基于 QPS/延时的权值计算，inflight delay 惩罚机制
- **MarkOld/ClearOld**：Remove 时跨双缓冲的并发权值追踪，无锁设计

详细文档见 [brpc_lalb/docs/](brpc_lalb/docs/)。

## 构建与测试

需要 Bazel。各子项目独立构建：

```bash
cd brpc_lalb
bazel build //...
bazel test //tests:all
```

## 代码规范

- Google C++ Style Guide + 禁用异常
- clang-format (BasedOnStyle: Google, ColumnLimit: 100)
- cpplint + clang-tidy 认知复杂度 <= 15
- 中文注释和文档
