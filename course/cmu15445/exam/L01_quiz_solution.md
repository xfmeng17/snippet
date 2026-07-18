# Lecture 01 随堂小测 答案与解析 / Quiz Solution: Relational Model & Algebra

> **仿真题，非 CMU 真题**
>
> 出题依据：Fall 2025 Lecture #01 课件 + 教材 Chapter 1-2

---

## Part A: 选择/判断题

### Q1 答案：B. 错误 / False

**解析**：关系模型中，relation 是 tuple 的**集合（set）**，集合中不允许有重复元素。因此一个 relation 中不能存在两个完全相同的 tuple。注意：SQL 中的表（table）默认是 **bag**（允许重复），但纯数学意义上的 relation 是 set。

> 考点出处：Lecture 01 课件 "Relation" 定义；教材 Ch 2.1

---

### Q2 答案：B

**解析**：Codd 在 1969 年提出关系模型，核心动机是 **逻辑-物理解耦**。在此之前的层次模型和网状模型中，应用代码直接依赖物理存储结构，物理层一变就要重写。关系模型通过提供抽象层（relation）解决了这个问题。

> 考点出处：Lecture 01 课件 "Relational Model" 章节；教材 Ch 1.3

---

### Q3 答案：C. ⋈ (Natural Join)

**解析**：关系代数的**基本运算**（fundamental / primitive operators）是：σ（Select）、π（Projection）、∪（Union）、−（Difference）、×（Cartesian Product）、ρ（Rename）。Natural Join（⋈）是**派生运算**，可以由 σ 和 × 组合表达：R ⋈ S = σ(R × S)。

> 考点出处：Lecture 01 课件关系代数运算符分类；教材 Ch 2.6

---

### Q4 答案：B

**解析**：σ（Select）是对行进行过滤，选出满足谓词的 tuple；π（Projection）是对列进行选择，只保留指定的属性。这是最基础的区分。

> 考点出处：Lecture 01 课件 σ 和 π 定义；教材 Ch 2.6.1

---

### Q5 答案：B. 错误 / False

**解析**：关系代数是**过程式（Procedural）**的 DML——它指定了操作的执行步骤/策略（先做什么运算、再做什么运算）。相对地，关系演算（Relational Calculus）是声明式的（只描述"要什么"，不描述"怎么做"）。SQL 总体上是声明式的，更接近关系演算。

> 考点出处：Lecture 01 课件 "Procedural vs. Non-Procedural" 对比；教材 Ch 2.6

---

### Q6 答案：D

**解析**：Candidate Key 是能唯一标识 tuple 的最小属性集合，一个关系可以有多个 candidate key。DBA 从中选择一个作为 Primary Key。此外，Primary Key 不允许 NULL，而一般 Candidate Key（如果不被选为 PK）在某些系统中可以有 NULL（取决于具体约束定义）。

> 考点出处：Lecture 01 课件 "Keys" 章节；教材 Ch 2.3

---

### Q7 答案：B. |R| × |S|

**解析**：Cartesian Product 将 R 中的每个 tuple 与 S 中的每个 tuple 配对。如果 R 有 m 行，S 有 n 行，结果有 m × n 行。属性数量 = R 的属性数 + S 的属性数。

> 考点出处：Lecture 01 课件 "Product (×)" 定义；教材 Ch 2.6.1

---

### Q8 答案：B. 错误 / False

**解析**：在关系模型的数学定义中，tuple 是属性名到值的**映射**，列的顺序是无关的。只要属性名和对应的值相同，tuple 的语义就相同。（SQL 实现中列有物理顺序，但关系模型的形式化定义中没有。）

> 考点出处：Lecture 01 课件 "Relation" 形式定义；教材 Ch 2.1-2.2

---

### Q9 答案：B

**解析**：Foreign Key 的定义就是：一个关系中的属性值必须对应另一个关系中某个 tuple 的主键值（参照完整性约束）。它用来建立关系之间的关联。

> 考点出处：Lecture 01 课件 "Keys" 章节；教材 Ch 2.3

---

### Q10 答案：B. 输入的关系（表）

**解析**：关系代数表达式的树形表示中，叶子节点是输入的 base relation，内部节点是关系代数运算符。数据从叶子（底部）向根（顶部）流动。这种表示方式也是后续查询优化器（query plan tree）的基础。

> 考点出处：Lecture 01 课件中"Relational Algebra" 示例的树形图；教材 Ch 2.6

---

## Part B: 简答题

### Q11 参考答案

```
π_name ( σ_{dept='CS' ∧ gpa>3.5} (R) )
```

或等价写法：

```
π_name ( σ_dept='CS' ( σ_gpa>3.5 (R) ) )
```

**评分标准**：
- σ 用对、谓词正确（3 分）
- π 用对、只投影 name（2 分）
- 运算顺序正确（先选后投影 或 合并谓词均可）

> 考点出处：Lecture 01 课件 σ + π 组合示例；教材 Ch 2.6.1

---

### Q12 参考答案

**过程式（Procedural）DML**：
- 用户需要指定**怎么做**——即给出获取数据的操作步骤/执行策略
- 代表：**关系代数（Relational Algebra）**
- 特点：用基本运算一步步组合，运算顺序影响语义

**声明式（Declarative / Non-Procedural）DML**：
- 用户只指定**要什么**——即描述期望的结果，不指定如何获取
- 代表：**SQL**（基于关系演算 Relational Calculus）
- 特点：DBMS 的查询优化器负责决定执行策略

**评分标准**：
- 正确区分"指定步骤" vs "只描述结果"（2 分）
- 各给出一个正确代表（2 分）
- 表述清晰完整（1 分）

> 考点出处：Lecture 01 课件 "DML" 分类；教材 Ch 2.6

---

*— End of Solution —*
