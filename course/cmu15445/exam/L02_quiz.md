# Lecture 02 随堂小测 / Quiz: Modern SQL

> **仿真题，非 CMU 真题** | 限时 20 分钟 | 满分 40 分
>
> 出题依据：Fall 2025 Lecture #02 课件 + 教材 Chapter 3-5
>
> 题目双语（中/英），答题语言不限

---

## Part A: 选择/判断题 (每题 3 分，共 30 分)

### Q1 [单选 / Single Choice]

SQL 中 `WHERE` 和 `HAVING` 的区别是：

The difference between `WHERE` and `HAVING` in SQL is:

- A. WHERE 过滤分组后的结果，HAVING 过滤原始行 / WHERE filters grouped results, HAVING filters raw rows
- B. WHERE 过滤原始行，HAVING 过滤分组后（GROUP BY 之后）的结果 / WHERE filters raw rows, HAVING filters grouped (post-GROUP BY) results
- C. 两者完全等价，可以互换使用 / They are completely equivalent and interchangeable
- D. WHERE 只能用于 SELECT，HAVING 只能用于 UPDATE / WHERE is only for SELECT, HAVING is only for UPDATE

---

### Q2 [判断 / True or False]

在 SQL 中，`SELECT COUNT(*)` 和 `SELECT COUNT(column_name)` 的结果一定相同。

In SQL, `SELECT COUNT(*)` and `SELECT COUNT(column_name)` always produce the same result.

- A. 正确 / True
- B. 错误 / False

---

### Q3 [单选 / Single Choice]

以下哪个是 Window Function 与普通 Aggregation 的关键区别？

Which of the following is the key difference between a Window Function and a regular Aggregation?

- A. Window Function 不能使用 SUM、AVG 等聚合函数 / Window functions cannot use aggregate functions like SUM, AVG
- B. Window Function 不会将多行折叠成单行输出 / Window functions do not collapse multiple rows into a single output row
- C. Window Function 只能用于数值类型 / Window functions can only be used with numeric types
- D. Window Function 不能指定排序 / Window functions cannot specify ordering

---

### Q4 [单选 / Single Choice]

以下 SQL 中 `ROW_NUMBER()`、`RANK()` 和 `DENSE_RANK()` 的区别是：
假设按成绩排序，有两个并列第 2 名。

For `ROW_NUMBER()`, `RANK()`, and `DENSE_RANK()` when ordering by grade with two students tied for 2nd place:

- A. ROW_NUMBER: 2,3; RANK: 2,2; DENSE_RANK: 2,2 — 下一名分别是 4, 4, 3 / next rank is 4, 4, 3
- B. 三者结果完全相同 / All three produce the same result
- C. ROW_NUMBER: 2,2; RANK: 2,3; DENSE_RANK: 2,2 / ROW_NUMBER: 2,2; RANK: 2,3; DENSE_RANK: 2,2
- D. ROW_NUMBER 和 RANK 相同，DENSE_RANK 不同 / ROW_NUMBER and RANK are the same, DENSE_RANK differs

---

### Q5 [判断 / True or False]

Common Table Expression（CTE，WITH 子句）本质上创建了一个永久表。

A Common Table Expression (CTE, WITH clause) essentially creates a permanent table.

- A. 正确 / True
- B. 错误 / False

---

### Q6 [单选 / Single Choice]

SQL 中的 `LIKE` 模式匹配里，`%` 和 `_` 分别代表：

In SQL's `LIKE` pattern matching, `%` and `_` represent:

- A. `%` 匹配任意单个字符，`_` 匹配任意长度字符串 / `%` matches any single character, `_` matches any string
- B. `%` 匹配任意长度字符串（含空串），`_` 匹配恰好一个字符 / `%` matches any string (including empty), `_` matches exactly one character
- C. 两者都匹配任意长度字符串 / Both match any-length strings
- D. `%` 只匹配数字，`_` 只匹配字母 / `%` matches digits only, `_` matches letters only

---

### Q7 [单选 / Single Choice]

下面哪种 SQL 嵌套查询（Nested Query）的用法是合法的？

Which of the following uses of nested queries (subqueries) is valid?

- A. 在 SELECT 子句中使用返回单值的子查询 / A subquery returning a single value in the SELECT clause
- B. 在 FROM 子句中使用返回表的子查询 / A subquery returning a table in the FROM clause
- C. 在 WHERE 子句中配合 IN、EXISTS 等使用子查询 / A subquery with IN, EXISTS in the WHERE clause
- D. 以上全部合法 / All of the above are valid

---

### Q8 [判断 / True or False]

递归 CTE（`WITH RECURSIVE`）使 SQL 成为图灵完备的语言。

Recursive CTEs (`WITH RECURSIVE`) make SQL a Turing-complete language.

- A. 正确 / True
- B. 错误 / False

---

### Q9 [单选 / Single Choice]

以下 SQL 查询的 `GROUP BY` 子句缺少哪个字段才会报错？

```sql
SELECT dept, name, COUNT(*)
FROM students
GROUP BY dept;
```

Which field is missing from the `GROUP BY` clause, causing this query to error?

- A. COUNT(*)
- B. name
- C. dept
- D. 不会报错 / It will not error

---

### Q10 [单选 / Single Choice]

SQL 中 `HAVING COUNT(*) > 5` 的执行时机是：

The execution timing of `HAVING COUNT(*) > 5` in SQL is:

- A. 在 FROM 获取数据之前 / Before FROM retrieves data
- B. 在 WHERE 过滤之前 / Before WHERE filtering
- C. 在 GROUP BY 分组之后、SELECT 输出之前 / After GROUP BY grouping, before SELECT output
- D. 在 ORDER BY 排序之后 / After ORDER BY sorting

---

## Part B: 简答题 (每题 5 分，共 10 分)

### Q11 [SQL 编写 / Write SQL]

给定表 `enrollment(sid, cid, grade)`，写一条 SQL 查询：
"找出每门课中成绩第二高的学生 ID。如果有并列，只要排名为 2 的。"

Given table `enrollment(sid, cid, grade)`, write a SQL query:
"Find the student ID with the second highest grade in each course. If there are ties, you want rank = 2."

（提示：使用 Window Function）

---

### Q12 [简答 / Short Answer]

解释 CTE（Common Table Expression）相比嵌套子查询（Nested Subquery）的两个优势，并写出一个简单的递归 CTE 示例（如生成 1 到 10 的数字序列）。

Explain two advantages of CTEs over nested subqueries, and write a simple recursive CTE example (e.g., generating a sequence of numbers from 1 to 10).

---

*— End of Quiz —*
