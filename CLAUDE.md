# snippet 项目开发指南

## 项目结构

- `brpc_lalb/` — brpc LALB（Locality-Aware Load Balancing）可读性重写，Bazel 构建，C++17

## C++ 编译/运行/测试

### 环境检测

如果当前机器有 OrbStack 的 `egg-dev` Linux Machine，**所有 C++ 编译、运行、测试必须在 `egg-dev` 内执行**，不在 macOS 本地执行。

检测方式：`orb list | grep egg-dev`

### 在 egg-dev 内执行（优先）

- **egg-dev 是 OrbStack Linux Machine**（Ubuntu 25.10 amd64），不是 Docker 容器
- 执行命令用 `orb run -m egg-dev`，不是 `docker exec`
- 路径映射：macOS `/Users/mengdandan` 直接挂载到 egg-dev 的 `/Users/mengdandan`（同路径）
- 可用 `-w` 指定工作目录，`-s` 使用 login shell

brpc_lalb 示例：
```bash
# 编译
orb run -m egg-dev -w /Users/mengdandan/project/snippet/brpc_lalb bazel build //...

# 全部测试
orb run -m egg-dev -w /Users/mengdandan/project/snippet/brpc_lalb bazel test //tests:all

# 单个测试
orb run -m egg-dev -w /Users/mengdandan/project/snippet/brpc_lalb bazel test //tests:<target>
```

### 本地执行（无 egg-dev 时的 fallback）

直接在 macOS 本地用 Bazel 构建：
```bash
cd brpc_lalb && bazel build //...
cd brpc_lalb && bazel test //tests:all
```

## 编码规范

- 遵循全局 CLAUDE.md 中的 C++ 编码规范（禁用异常、返回值报错等）
- Bazel 构建，C++17 标准
- 中文注释和文档
