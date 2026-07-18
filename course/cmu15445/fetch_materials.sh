#!/usr/bin/env bash
# 下载 CMU 15-445/645 Fall 2025 的公开课程材料（课件 / 笔记 / 书面作业）
#
# 用途：换机器时重跑一遍即可，不用手点。
# 注意：这些 PDF 是 CMU 的版权材料，下载自用没问题，
#       但【不要】把 PDF 提交进任何 git 仓库（本脚本的输出目录不在仓库里）。
#
# 用法:  bash fetch_materials.sh [目标目录]
#        默认目标目录: ~/project/cmu15445-fall2025

set -euo pipefail

BASE="https://15445.courses.cs.cmu.edu/fall2025"
DEST="${1:-$HOME/project/cmu15445-fall2025}"

mkdir -p "$DEST"/{slides,notes,homework,solutions}

# 25 讲的课件与课后标注笔记，文件名与官方 schedule 页一致
LECTURES=(
  "01-relationalmodel" "02-modernsql"        "03-storage1"        "04-bufferpool"
  "05-storage2"        "06-storage3"         "07-hashtables"      "08-indexes1"
  "09-indexes2"        "10-indexconcurrency" "11-sorting"         "12-joins"
  "13-queryexecution1" "14-queryexecution2"  "15-optimization1"   "16-optimization2"
  "17-concurrencycontrol" "18-twophaselocking" "19-timestampordering"
  "20-multiversioning" "21-logging"          "22-recovery"
  "23-distributed1"    "24-distributed2"     "25-potpourri"
)

echo "==> 下载课件 slides/"
for l in "${LECTURES[@]}"; do
  curl -fsSL --retry 3 -o "$DEST/slides/$l.pdf" "$BASE/slides/$l.pdf" \
    && echo "    ok  $l.pdf" \
    || echo "    --  $l.pdf (不存在，跳过)"
done

echo "==> 下载课后标注笔记 notes/"
for l in "${LECTURES[@]}"; do
  curl -fsSL --retry 3 -o "$DEST/notes/$l.pdf" "$BASE/notes/$l.pdf" \
    && echo "    ok  $l.pdf" \
    || echo "    --  $l.pdf (不存在，跳过)"
done

# HW1 是在线 SQL 作业，没有 PDF；HW2-HW6 有题目(clean)和答案(sols)
echo "==> 下载书面作业 homework/ 与答案 solutions/"
for i in 2 3 4 5 6; do
  curl -fsSL --retry 3 -o "$DEST/homework/hw$i.pdf"      "$BASE/files/hw$i-clean.pdf" \
    && echo "    ok  hw$i.pdf"
  curl -fsSL --retry 3 -o "$DEST/solutions/hw$i-sols.pdf" "$BASE/files/hw$i-sols.pdf" \
    && echo "    ok  hw$i-sols.pdf (答案，做完再看)"
done

echo
echo "完成。材料在: $DEST"
echo "  slides/     课堂讲义（配着视频看）"
echo "  notes/      课后标注版（讲完之后 Andy 补的批注，复习用）"
echo "  homework/   书面作业题目（HW1 是在线 SQL 题，无 PDF）"
echo "  solutions/  官方答案 —— 做完再看"
echo
echo "HW1 (SQL) 在线做: $BASE/homework1/"
