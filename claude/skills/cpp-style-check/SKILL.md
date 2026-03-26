---
name: cpp-style-check
version: "1.1.0"
description: 检查 C++ 代码的 Google 风格规范和认知复杂度。使用 cpplint 做 Google C++ Style Guide 检查，clang-tidy 做认知复杂度检测，clang-format 做格式化，还包含自定义规则（禁止异常、限制 auto 等）。当用户提到"C++风格检查"、"代码规范"、"cpplint"、"clang-tidy"、"cpp check"、"style check"时使用此技能。
allowed-tools:
  - Bash(*)
  - Read
  - Write
  - Edit
  - Glob
  - AskUserQuestion
---

# C++ 代码风格检查

> **Version**: 1.1.0

## 概览

三层检查体系：
1. **clang-format** — 代码格式化（缩进、换行、对齐）
2. **cpplint** — Google C++ Style Guide 规范检查（命名、头文件、作用域等）
3. **clang-tidy** — 认知复杂度 + 静态分析（复杂度阈值、google-* 检查）
4. **自定义规则** — 项目特有的编码约定（禁止异常、限制 auto 等）

## 工作流程

**检查 C++ 代码时，按顺序执行：**

1. **格式化检查**：用 `clang-format --dry-run -Werror` 检查格式，或直接 `clang-format -i` 格式化
2. **风格检查**：运行 `cpplint` 检查 Google C++ 风格规范
3. **复杂度检查**：运行 `clang-tidy` 检查认知复杂度
4. **自定义规则检查**：用脚本检查项目特有的编码规则
5. **汇总报告**：整合所有结果，给出修复建议
6. **修复代码**：按报告逐项修复
7. **验证通过**：重新运行检查

## 运行环境

> **重要**: C++ 项目的编译和检查应在 Linux 环境（OrbStack Linux Machine）中执行，不要在 macOS 原生运行。
> 使用 `orbctl run -m <machine>` 在 Linux Machine 中执行检查命令。

## 前置依赖

# macOS (仅安装工具本身，实际检查在 Linux 中运行)
brew install llvm cpplint

# Linux (OrbStack Linux Machine / Ubuntu)
sudo apt install clang-format clang-tidy python3-pip
python3 -m pip install --break-system-packages cpplint

## 检测命令

### 1. clang-format（格式化）

# 检查格式是否合规（不修改文件）
clang-format --dry-run -Werror --style=file path/to/file.cc

# 直接格式化
clang-format -i --style=file path/to/file.cc

# 格式化整个目录
find src/ -name '*.cc' -o -name '*.h' | xargs clang-format -i --style=file

> 项目根目录的 `.clang-format` 文件定义格式规则（BasedOnStyle: Google）。

### 2. cpplint（Google 风格规范）

# 检查单个文件
cpplint --filter=-legal/copyright path/to/file.cc

# 推荐 filter
cpplint --filter=-legal/copyright,-build/include_subdir,-build/c++11,-build/c++17,-build/header_guard,-build/include_order,-whitespace/indent_namespace,-runtime/string path/to/file.cc

### 3. clang-tidy（认知复杂度 + 静态分析）

# 检查认知复杂度（阈值 15）
clang-tidy -checks='-*,readability-function-cognitive-complexity' \
  -config='{CheckOptions: [{key: readability-function-cognitive-complexity.Threshold, value: 15}]}' \
  path/to/file.cc -- -std=c++17

### 4. 自定义规则检查（脚本）

python3 ~/.claude/skills/cpp-style-check/scripts/check_custom_rules.py path/to/file.cc

### 5. 一键全检查

python3 ~/.claude/skills/cpp-style-check/scripts/check_all.py path/to/file_or_dir \
  --complexity-threshold 15 --linelength 100

## Google C++ Style Guide 核心规则速查

> 基于 [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)（2026 版，目标 C++20）

### 命名规范

| 类型 | 风格 | 示例 |
|------|------|------|
| 文件名 | 小写+下划线 | `my_class.h`, `my_class.cc` |
| 类/结构体 | CapWords | `MyClass`, `UrlParser` |
| 函数 | CapWords | `GetValue()`, `set_value()` (accessor) |
| 变量 | 小写+下划线 | `table_name`, `num_errors` |
| 类成员变量 | 末尾下划线 | `table_name_`, `num_errors_` |
| 常量 | k 前缀 + CapWords | `kMaxSize`, `kDaysInWeek` |
| 枚举值 | k 前缀 + CapWords | `kOk`, `kNotFound` |
| 宏 | 全大写+下划线 | `MY_MACRO_NAME` |
| 命名空间 | 小写+下划线 | `my_project`, `web_search` |

### 头文件规则

- 每个 `.cc` 对应一个 `.h`
- Header guard 格式: `#ifndef PROJECT_PATH_FILE_H_`（或用 `#pragma once`）
- include 顺序（各组之间空行分隔）:
  1. 对应的 `.h` 文件
  2. C 系统头文件（`<unistd.h>`）
  3. C++ 标准库头文件（`<string>`）
  4. 第三方库头文件
  5. 项目内头文件

### 类设计

- `struct` 仅用于纯数据容器（全 public 字段）；有行为用 `class`
- 数据成员设为 `private`
- 显式声明或 `= delete` 拷贝/移动操作
- override 标记虚函数覆写，构造函数中不调虚函数
- 单参数构造函数标记 `explicit`
- 优先组合而非继承

### 函数设计

- 函数保持短小（约 40 行以内）
- 优先用返回值而非输出参数
- 返回值优先级: 值 > 引用 > 指针（仅可空时用指针）
- 输入参数在前，输出参数在后
- 虚函数禁止默认参数

### 类型与转换

- 用 C++ 风格 cast: `static_cast`, `const_cast`, `reinterpret_cast`（禁止 C 风格强转）
- 避免 RTTI（`dynamic_cast`, `typeid`）
- 用 `std::bit_cast` 做类型双关

### 智能指针

- 独占所有权用 `std::unique_ptr`
- 共享所有权用 `std::shared_ptr`（尽量避免）
- 裸指针仅表示非拥有引用

### 禁止/限制的特性

- **禁止 C++ 异常**（`throw`, `try-catch`）
- 禁止 `using namespace xxx`（using-directive）
- 禁止 `goto`
- 禁止用户定义字面量（UDL）
- 禁止重载 `&&`, `||`, `,`, `&`（一元取地址）
- 避免虚继承和多继承
- 限制模板元编程

### 2025-2026 重要更新

- **允许可变引用参数** — 不再要求输出参数必须用指针
- **允许 C++20 指定初始化** — `Foo{.bar = 42, .baz = "hello"}`
- **大括号规则更精确** — `if/for/while` 单行语句体的 `{}` 规则有更明确的指导

## 自定义规则

> 这些规则是 Google 风格的补充/强化，通过自定义脚本检查。

### 规则 1: 禁止 try-catch 和异常机制

**级别**: ERROR

与 Google 风格一致，但更严格地检测：
- `try {`, `catch (`, `throw ` 关键字
- `std::exception_ptr`, `std::current_exception`, `std::rethrow_exception`

### 规则 2: 限制 auto 使用

**级别**: WARNING

仅允许 `auto` 用于:
- 迭代器类型: `auto it = map.begin();`
- lambda 表达式: `auto fn = [](int x) { ... };`

### 规则 3: 强制大括号

**级别**: WARNING

`for`, `if`, `while`, `else` 语句体必须使用 `{}`，即使只有一行。

### 规则 4: 禁止抛异常的数值解析函数

**级别**: ERROR

禁止 `std::stoi`, `std::stol`, `std::stof`, `std::stod` 等会抛异常的函数。

## 软件工程原则: 复杂度管理

> "管理复杂度是软件开发的首要技术使命。"  —— Steve McConnell,《代码大全》

### 圈复杂度 vs 认知复杂度

**圈复杂度 (Cyclomatic Complexity)**
- 由 Thomas McCabe 于 1976 年提出
- 衡量代码中**独立执行路径**的数量
- 关注**可测试性**

**认知复杂度 (Cognitive Complexity)**
- 由 SonarSource 于 2017 年提出
- 衡量**人类理解代码的心理负担**
- 关注**可读性和可维护性**，考虑嵌套深度的惩罚

### 卫语句 (Guard Clause)

> "卫语句就是在函数开头就退出的代码，避免深层嵌套。"  —— McConnell,《代码大全》第392页

### 《代码大全》核心经验法则

1. 函数长度控制: 不超过 40 行
2. 嵌套深度限制: 不超过 3~4 层
3. 函数参数数量: 不超过 7 个
4. 圈复杂度阈值: 不超过 10~15
5. 单一职责: 一个函数只做一件事
6. 防御式编程: 在函数入口检查前置条件
7. 变量作用域最小化

## 认知复杂度计算规则（C++ 版）

- 每个 `if`, `else if`, `else`: +1
- 每个 `for`, `while`, `do-while`: +1
- 每个 `switch`, `case`: +1
- 每个 `catch`: +1
- 每个 `goto`: +1（额外惩罚）
- 每层嵌套: 额外 +1（嵌套惩罚）
- 递归调用: +1
- `&&`, `||` 条件序列: +1
- 三元运算符 `?:`: +1

## 复杂度阈值参考

| 指标 | 优秀 | 良好 | 需关注 | 必须重构 |
|------|------|------|--------|----------|
| 认知复杂度 | 1-5 | 6-10 | 11-15 | >15 |
| 圈复杂度 | 1-5 | 6-10 | 11-15 | >15 |
| 函数行数 | <20 | 20-40 | 40-60 | >60 |
| 嵌套深度 | 1-2 | 3 | 4 | >4 |
| 参数个数 | 0-3 | 4-5 | 6-7 | >7 |

## C++ 重构策略

1. 提取子函数
2. 卫语句 (Guard Clause) — 早返回替代嵌套
3. 用 map/数组替代多分支 switch
4. 分解复杂条件
5. 用多态替代类型判断

## 检测失败时的处理

如果重构后仍无法降低复杂度到阈值以下，使用 NOLINT 标注说明。

## 参考资源

- 《Code Complete》(代码大全), Steve McConnell
- 《Refactoring》(重构), Martin Fowler
- [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)
- [cpplint (GitHub)](https://github.com/cpplint/cpplint)
- [clang-tidy readability-function-cognitive-complexity](https://clang.llvm.org/extra/clang-tidy/checks/readability/function-cognitive-complexity.html)
- [SonarSource: Cognitive Complexity](https://www.sonarsource.com/docs/CognitiveComplexity.pdf)
- [DeepWiki: Google C++ Style Guide](https://deepwiki.com/google/styleguide/2.2-c++-style-guide)
