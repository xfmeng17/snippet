# Lecture 02 随堂小测 答案与解析 / Quiz Solution: Modern SQL

> **仿真题，非 CMU 真题**
>
> 出题依据：Fall 2025 Lecture #02 课件 + 教材 Chapter 3-5

---

## Part A: 选择/判断题

### Q1 答案：B

**解析**：SQL 的执行逻辑顺序为 FROM → WHERE → GROUP BY → HAVING → SELECT → ORDER BY。WHERE 在分组之前过滤行，HAVING 在分组之后过滤组。HAVING 中可以使用聚合函数（如 COUNT、SUM），WHERE 中不行。

> 考点出处：Lecture 02 课件 "HAVING" 章节；教材 Ch 3.7

---

### Q2 答案：B. 错误 / False

**解析**：`COUNT(*)` 计算所有行数（包括含 NULL 的行），而 `COUNT(column_name)` 只计算该列**非 NULL** 的行数。如果 column_name 列中有 NULL 值，两者结果不同。

> 考点出处：Lecture 02 课件 "Aggregation" 章节；教材 Ch 3.7

---

### Q3 答案：B

**解析**：Window Function 对一组相关的行执行计算，但**不会**像 GROUP BY + 聚合那样把多行折叠成一行。每个输入行在输出中仍然保持独立存在，只是额外附加了窗口计算结果。

> 考点出处：Lecture 02 课件 "Window Functions" 章节；教材 Ch 5.5

---

### Q4 答案：A

**解析**：
- `ROW_NUMBER()`：给每行分配唯一递增编号，并列时也不同（2, 3）
- `RANK()`：并列时给相同排名，但下一名跳号（2, 2, 4）
- `DENSE_RANK()`：并列时给相同排名，下一名不跳号（2, 2, 3）

所以两个并列第 2 名后：ROW_NUMBER 下一个是 4，RANK 下一个是 4，DENSE_RANK 下一个是 3。

> 考点出处：Lecture 02 课件 "Window Functions" 章节；教材 Ch 5.5

---

### Q5 答案：B. 错误 / False

**解析**：CTE 是一个**临时的、作用域仅限于当前查询的**结果集。它不会创建永久表，查询执行完毕后 CTE 就消失了。可以将其理解为一个"命名的子查询"。

> 考点出处：Lecture 02 课件 "Common Table Expressions" 章节；教材 Ch 5.6

---

### Q6 答案：B

**解析**：这是 SQL 字符串模式匹配的基础：`%` 匹配任意长度字符串（包括空串），`_` 恰好匹配一个字符。例如 `'___%'` 匹配长度 ≥ 3 的任意字符串。

> 考点出处：Lecture 02 课件 "String Operations" 章节；教材 Ch 3.4.2

---

### Q7 答案：D. 以上全部合法

**解析**：SQL 子查询可以出现在多个位置：
- **SELECT 子句**：返回单值的标量子查询
- **FROM 子句**：返回一个派生表（derived table）
- **WHERE 子句**：配合 IN、EXISTS、ANY、ALL 等谓词

这三种用法在 SQL 标准中均合法。

> 考点出处：Lecture 02 课件 "Nested Queries" 章节；教材 Ch 3.8

---

### Q8 答案：A. 正确 / True

**解析**：这是 Lecture 02 的一个关键结论。递归 CTE（`WITH RECURSIVE`）允许 SQL 表达任意递归计算，这使得 SQL 成为图灵完备的语言——理论上可以计算任何可计算函数。

> 考点出处：Lecture 02 课件 "Common Table Expressions - Recursive" 章节

---

### Q9 答案：B. name

**解析**：SQL 规则：SELECT 子句中出现的**非聚合列**必须在 GROUP BY 中列出。这里 `dept` 在 GROUP BY 中，`COUNT(*)` 是聚合，但 `name` 既不是聚合也没出现在 GROUP BY 中，违规。（注：MySQL 默认宽松模式可能不报错，但 SQL 标准和大多数 DBMS 会报错。）

> 考点出处：Lecture 02 课件 "Aggregation" 章节；教材 Ch 3.7

---

### Q10 答案：C

**解析**：SQL 的逻辑执行顺序：
1. FROM（获取数据）
2. WHERE（过滤行）
3. GROUP BY（分组）
4. **HAVING**（过滤组）← 在这里
5. SELECT（计算输出列）
6. ORDER BY（排序）

HAVING 在 GROUP BY 之后执行，用于对分组后的结果进行条件过滤。

> 考点出处：Lecture 02 课件 "HAVING" 章节；教材 Ch 3.7

---

## Part B: 简答题

### Q11 参考答案

```sql
SELECT cid, sid
FROM (
    SELECT cid, sid, grade,
           DENSE_RANK() OVER (PARTITION BY cid ORDER BY grade DESC) AS rk
    FROM enrollment
) sub
WHERE rk = 2;
```

**注意**：题目说"排名为 2"，用 `DENSE_RANK()` 或 `RANK()` 均可接受。如果用 `ROW_NUMBER()`，并列时可能只返回其中一个。

**评分标准**：
- 正确使用 Window Function + OVER + PARTITION BY（2 分）
- ORDER BY grade DESC 正确（1 分）
- 外层 WHERE rk = 2 正确筛选（1 分）
- 整体语法正确可执行（1 分）

> 考点出处：Lecture 02 课件 "Window Functions" 示例；教材 Ch 5.5

---

### Q12 参考答案

**CTE 相比嵌套子查询的两个优势：**

1. **可读性（Readability）**：CTE 将复杂查询分解为命名的逻辑块，从上到下阅读，比多层嵌套的子查询更清晰。
2. **可复用（Reusability）**：同一个 CTE 可以在主查询中多次引用，而嵌套子查询如果要复用就必须复制粘贴（或让 DBMS 自动识别公共子表达式）。

其他可接受的答案：递归能力（嵌套子查询不支持递归）、便于调试和维护。

**递归 CTE 示例（生成 1-10）：**

```sql
WITH RECURSIVE seq(n) AS (
    -- Base case
    SELECT 1
    UNION ALL
    -- Recursive step
    SELECT n + 1
    FROM seq
    WHERE n < 10
)
SELECT n FROM seq;
```

**评分标准**：
- 正确给出两个优势并解释（2 分）
- 递归 CTE 语法正确：WITH RECURSIVE + base case + UNION ALL + recursive step + 终止条件（3 分）

> 考点出处：Lecture 02 课件 "Common Table Expressions" 章节；教材 Ch 5.6

---

*— End of Solution —*
