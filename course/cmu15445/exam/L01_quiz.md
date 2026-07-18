# Lecture 01 随堂小测 / Quiz: Relational Model & Algebra

> **仿真题，非 CMU 真题** | 限时 20 分钟 | 满分 40 分
>
> 出题依据：Fall 2025 Lecture #01 课件 + 教材 Chapter 1-2
>
> 题目双语（中/英），答题语言不限

---

## Part A: 选择/判断题 (每题 3 分，共 30 分)

### Q1 [判断 / True or False]

关系模型中，一个 relation（关系）中可以存在两个完全相同的 tuple（元组）。

In the relational model, a relation can contain two identical tuples.

- A. 正确 / True
- B. 错误 / False

---

### Q2 [单选 / Single Choice]

Ted Codd 提出关系模型的核心动机是什么？

What was Ted Codd's primary motivation for proposing the relational model?

- A. 提高查询性能 / Improve query performance
- B. 将逻辑层与物理层解耦，避免物理存储变化时重写应用 / Decouple the logical layer from the physical layer to avoid rewriting applications when physical storage changes
- C. 支持并发事务 / Support concurrent transactions
- D. 减少存储空间 / Reduce storage space

---

### Q3 [单选 / Single Choice]

以下哪个不是关系代数的基本运算（fundamental operator）？

Which of the following is NOT a fundamental operator of relational algebra?

- A. σ (Select)
- B. π (Projection)
- C. ⋈ (Natural Join)
- D. ∪ (Union)

---

### Q4 [单选 / Single Choice]

关系代数中，Select（σ）和 Projection（π）的区别是：

In relational algebra, the difference between Select (σ) and Projection (π) is:

- A. σ 选择列，π 选择行 / σ selects columns, π selects rows
- B. σ 选择行，π 选择列 / σ selects rows, π selects columns
- C. σ 作用于多个关系，π 作用于单个关系 / σ operates on multiple relations, π operates on a single relation
- D. 两者功能相同 / They have the same function

---

### Q5 [判断 / True or False]

关系代数是声明式（declarative）语言。

Relational algebra is a declarative language.

- A. 正确 / True
- B. 错误 / False

---

### Q6 [单选 / Single Choice]

Primary Key 和 Candidate Key 的关系是：

The relationship between a Primary Key and a Candidate Key is:

- A. Primary Key 是 Candidate Key 的子集 / A Primary Key is a subset of a Candidate Key
- B. Primary Key 是从所有 Candidate Key 中选出的一个 / A Primary Key is one chosen from all Candidate Keys
- C. Candidate Key 可以包含 NULL，Primary Key 不行 / Candidate Keys can contain NULL but Primary Keys cannot
- D. B 和 C 都对 / Both B and C are correct

---

### Q7 [单选 / Single Choice]

关系代数中，Cartesian Product（笛卡尔积，×）的结果中元组数量是：

In relational algebra, the number of tuples in the result of a Cartesian Product (×) is:

- A. |R| + |S|
- B. |R| × |S|
- C. max(|R|, |S|)
- D. |R| - |S|

---

### Q8 [判断 / True or False]

关系模型中，tuple 的属性值顺序（列的排列顺序）是有意义的，交换两列后语义不同。

In the relational model, the order of attribute values (column ordering) in a tuple is significant — swapping two columns changes the semantics.

- A. 正确 / True
- B. 错误 / False

---

### Q9 [单选 / Single Choice]

Foreign Key（外键）的作用是：

The purpose of a Foreign Key is:

- A. 保证本表内没有重复元组 / Ensure no duplicate tuples within the same table
- B. 指定一个属性必须映射到另一个关系中的某个元组 / Specify that an attribute must map to a tuple in another relation
- C. 自动创建索引以加速查询 / Automatically create an index to speed up queries
- D. 定义属性的数据类型 / Define the data type of an attribute

---

### Q10 [单选 / Single Choice]

关系代数表达式可以用"树"的形式表示。在这种表示中，叶子节点是什么？

Relational algebra expressions can be represented as a tree. In this representation, what are the leaf nodes?

- A. 运算符 / Operators
- B. 输入的关系（表）/ Input relations (tables)
- C. 输出的元组 / Output tuples
- D. 谓词条件 / Predicates

---

## Part B: 简答题 (每题 5 分，共 10 分)

### Q11 [简答 / Short Answer]

给定关系 R(sid, name, gpa, dept)，写出关系代数表达式：
"找出 CS 系中 GPA > 3.5 的学生的姓名"。

Given relation R(sid, name, gpa, dept), write a relational algebra expression:
"Find the names of students in the CS department with GPA > 3.5."

---

### Q12 [简答 / Short Answer]

解释 DML 中"过程式"（Procedural）和"声明式"（Declarative/Non-Procedural）两种方式的区别，并分别举出一个代表性的语言/系统。

Explain the difference between "Procedural" and "Declarative (Non-Procedural)" approaches in DML, and give one representative language/system for each.

---

*— End of Quiz —*
