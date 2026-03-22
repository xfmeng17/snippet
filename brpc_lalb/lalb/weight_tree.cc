#include "lalb/weight_tree.h"

#include <random>

namespace lalb {

// ============================================================
// 线程局部随机数生成器
// ============================================================
static int64_t FastRandLessThan(int64_t range) {
  if (range <= 0) return 0;
  thread_local std::mt19937_64 rng(std::random_device{}());
  return std::uniform_int_distribution<int64_t>(0, range - 1)(rng);
}

// ============================================================
// Servers::UpdateParentWeights
//
// 从 index 向根遍历，如果 index 是某个父节点的左子节点，
// 就把 diff 加到该父节点的 left_weight 上。
// 用 atomic fetch_add (relaxed)：不需要强一致性，只要最终一致。
//
// 为什么 relaxed 就够了？
// - left_weight 只影响 Select 的查找路径
// - 如果某次 Select 看到了稍微过时的 left_weight，最多导致选择偏差
// - 这种偏差在下次 Select 时就会被纠正
// - 如果用 acquire/release，每次更新都会在所有核之间同步 cache line，
//   而 LALB 的 Select 非常频繁，这个成本不可接受
// ============================================================
void Servers::UpdateParentWeights(int64_t diff, size_t index) const {
  while (index != 0) {
    size_t parent = (index - 1) >> 1;
    if ((parent << 1) + 1 == index) {
      // index 是左子节点 → 更新父节点的 left_weight
      weight_tree[parent].left_weight->fetch_add(diff,
                                                 std::memory_order_relaxed);
    }
    index = parent;
  }
}

// ============================================================
// left_weight 管理
// ============================================================
std::atomic<int64_t>* WeightTree::PushLeft() {
  left_weights_.emplace_back(0);
  return &left_weights_.back();
}

void WeightTree::PopLeft() { left_weights_.pop_back(); }

// ============================================================
// Add: 向后台 buffer 添加 server
//
// 双缓冲的关键设计：
// 第一次调用（fg 中没有 id）:
//   - 创建新的 Weight 对象和 left_weight 条目
//   - 计算初始权值（已有节点的平均值）
//   - 更新父节点的 left_weight
//   - 更新 total_weight
//
// 第二次调用（fg 中有 id，因为第一次已经添加了）:
//   - 直接从 fg 复制 TreeNode（shared_ptr 引用同一个 Weight）
//   - 不需要重新创建 Weight，也不需要再更新父节点
//   - 因为 Weight 和 left_weight 是共享的，第一次的修改已经生效
//
// 为什么需要 ModifyWithForeground？
// 因为 Weight 对象很重（有互斥锁、循环队列等），不能创建两份。
// 通过检查 fg 中是否已有 id，第二次调用可以直接复制指针。
// ============================================================
bool WeightTree::Add(Servers& bg, const Servers& fg, uint64_t server_id,
                     WeightTree* self, int64_t* out_weight) {
  if (bg.server_map.count(server_id)) {
    return false;  // 重复
  }

  std::unordered_map<uint64_t, size_t>::const_iterator fg_it =
      fg.server_map.find(server_id);

  if (fg_it == fg.server_map.end()) {
    // ---- 第一次调用：创建新节点 ----
    size_t index = bg.weight_tree.size();

    // 初始权值：已有节点的平均权值，或 WEIGHT_SCALE
    int64_t initial_weight = Weight::kWeightScale;
    if (!bg.weight_tree.empty()) {
      int64_t total = 0;
      for (size_t i = 0; i < bg.weight_tree.size(); ++i) {
        total += bg.weight_tree[i].weight->volatile_value();
      }
      initial_weight = total / static_cast<int64_t>(bg.weight_tree.size());
      if (initial_weight <= 0) {
        initial_weight = Weight::kWeightScale;
      }
    }

    bg.server_map[server_id] = index;

    TreeNode node;
    node.server_id = server_id;
    node.left_weight = self->PushLeft();
    node.weight = std::make_shared<Weight>(initial_weight);
    bg.weight_tree.push_back(node);

    // 更新父节点
    int64_t w = node.weight->volatile_value();
    if (w > 0) {
      bg.UpdateParentWeights(w, index);
    }
    // 记录实际分配的权值（仅第一次调用时写入）
    *out_weight = w;
  } else {
    // ---- 第二次调用：从前台复制 ----
    bg.server_map[server_id] = bg.weight_tree.size();
    bg.weight_tree.push_back(fg.weight_tree[fg_it->second]);
  }
  return true;
}

// ============================================================
// Remove: 从后台 buffer 删除 server
//
// 这是 LALB 中并发设计最复杂的部分。
//
// 简单情况（删除末尾节点）:
//   直接 pop_back 即可。
//
// 复杂情况（删除非末尾节点）:
//   必须把末尾节点搬到被删位置来保持完全二叉树的结构。
//
//   ---- 第一次调用（后台 buffer）----
//   1. Disable(被删节点): 将权值置 0，返回旧权值 rm_weight (> 0)
//   2. 搬移末尾节点到被删位置
//   3. MarkOld(tree_size): 记住末尾节点当前权值和旧位置
//      → 之后前台读线程在旧位置调用 ResetWeight 时，diff 会累积到 old_diff_sum
//   4. 更新被删位置的父节点: diff = add_weight - rm_weight
//      注意：不动末尾位置的父节点！因为前台还在用旧树结构，
//      前台看到的末尾节点还在末尾位置，移除它会导致前台 Select 越界。
//
//   ---- 翻转 + 等待所有读线程切换 ----
//
//   ---- 第二次调用（新后台 = 原前台）----
//   1. Disable(被删节点): 返回 0（已经 disabled）→ 进入第二次逻辑
//   2. 搬移末尾节点到被删位置（和第一次一样）
//   3. ClearOld(): 获取 {old_weight, accumulated_diff}
//      - old_weight: MarkOld 时的权值
//      - accumulated_diff: 两次修改之间前台产生的权值变化
//   4. 更新被删位置的父节点: diff = accumulated_diff
//      这个 diff 是前台读线程在旧位置产生的权值变化，需要应用到新位置
//   5. 更新末尾位置的父节点: old_weight = -(old_weight + accumulated_diff)
//      把旧位置的权值完全移除
//   6. 更新 total_weight: -old_weight
//   7. 释放资源（被删节点的 Weight，末尾位置的 left_weight）
// ============================================================
bool WeightTree::Remove(Servers& bg, uint64_t server_id, WeightTree* self,
                        int64_t* out_weight) {
  std::unordered_map<uint64_t, size_t>::iterator it =
      bg.server_map.find(server_id);
  if (it == bg.server_map.end()) {
    return false;
  }

  size_t index = it->second;
  bg.server_map.erase(it);

  std::shared_ptr<Weight> w = bg.weight_tree[index].weight;
  int64_t rm_weight = w->Disable();

  // 第一次调用时记录被移除节点的权值
  if (rm_weight > 0) {
    *out_weight = rm_weight;
  }

  if (index + 1 == bg.weight_tree.size()) {
    // ---- 简单情况：删除末尾节点 ----
    bg.weight_tree.pop_back();
    if (rm_weight > 0) {
      // 第一次修改：移除权值但不释放资源
      // （前台可能还在通过 left_weight 指针访问末尾位置）
      bg.UpdateParentWeights(-rm_weight, index);
    } else {
      // 第二次修改：释放资源
      self->PopLeft();
    }
  } else {
    // ---- 复杂情况：删除非末尾节点，需要搬移 ----

    // 搬移末尾节点到被删位置
    // 注意：只搬 server_id 和 weight，left_weight 不动（和位置绑定）
    bg.weight_tree[index].server_id = bg.weight_tree.back().server_id;
    bg.weight_tree[index].weight = bg.weight_tree.back().weight;
    bg.server_map[bg.weight_tree[index].server_id] = index;
    bg.weight_tree.pop_back();

    std::shared_ptr<Weight> w2 = bg.weight_tree[index].weight;  // 搬过来的

    if (rm_weight > 0) {
      // ---- 第一次修改（后台 buffer）----

      // MarkOld: 记住搬移节点的当前权值和旧位置
      // bg.weight_tree.size() 就是搬移前的末尾索引（已 pop_back）
      int64_t add_weight = w2->MarkOld(bg.weight_tree.size());

      // 在被删位置更新父节点：新权值 - 旧权值
      // 不动末尾位置的父节点（前台还在用）
      int64_t diff = add_weight - rm_weight;
      if (diff != 0) {
        bg.UpdateParentWeights(diff, index);
      }
    } else {
      // ---- 第二次修改（新后台 = 原前台）----

      // ClearOld: 获取两次修改之间的累积变化
      std::pair<int64_t, int64_t> p = w2->ClearOld();
      int64_t old_weight = p.first;
      int64_t accumulated_diff = p.second;

      // 1. 把累积的 diff 应用到被删位置的父节点
      if (accumulated_diff != 0) {
        bg.UpdateParentWeights(accumulated_diff, index);
      }

      // 2. 从末尾位置移除权值
      //    末尾位置的实际权值 = old_weight + accumulated_diff
      //    需要移除的量 = -(old_weight + accumulated_diff)
      int64_t remove_from_old = -(old_weight + accumulated_diff);
      if (remove_from_old != 0) {
        bg.UpdateParentWeights(remove_from_old, bg.weight_tree.size());
      }

      // 3. 释放资源
      self->PopLeft();
    }
  }
  return true;
}

// ============================================================
// AddServer / RemoveServer: 公开接口
// ============================================================

int64_t WeightTree::AddServer(uint64_t server_id) {
  int64_t assigned_weight = 0;
  bool ok = db_servers_.ModifyWithForeground(
      [server_id, this, &assigned_weight](Servers& bg,
                                          const Servers& fg) -> bool {
        return Add(bg, fg, server_id, this, &assigned_weight);
      });
  return ok ? assigned_weight : 0;
}

int64_t WeightTree::RemoveServer(uint64_t server_id) {
  int64_t removed_weight = 0;
  bool ok = db_servers_.Modify(
      [server_id, this, &removed_weight](Servers& bg) -> bool {
        return Remove(bg, server_id, this, &removed_weight);
      });
  return ok ? removed_weight : 0;
}

// ============================================================
// Select: 按权值选择 server
//
// 读路径：通过 DoublyBufferedData::Read 获取前台快照。
// 整个 Select 过程中，thread-local 锁被持有，保证前台不会被翻转。
//
// 查找算法：
// 1. 生成随机数 dice ∈ [0, total_weight)
// 2. 从根节点开始：
//    - dice < left_weight → 走左子树
//    - dice >= left_weight + self_weight → 减去后走右子树
//    - 否则 → 命中当前节点
// 3. 命中后调用 AddInflight:
//    - 重算权值（可能因 inflight delay 变化）
//    - 如果权值够大（>= dice - left），选中该节点
//    - 如果权值太小（被 inflight delay 惩罚），重新 roll dice
// 4. 最多尝试 n 次，全部失败则随机兜底
//
// 注意事项：
// - left_weight / self_weight / total_weight 可能不一致（原子操作之间有间隙）
// - 这是故意的：追求一致性需要加锁，但 LALB 的设计哲学是"最终一致就够了"
// - 偶尔的选择偏差会在后续 Feedback 中被纠正
// ============================================================
WeightTree::SelectResult WeightTree::Select(int64_t total_weight,
                                            int64_t begin_time_us) {
  DoublyBufferedData<Servers>::ScopedPtr s;
  if (db_servers_.Read(&s) != 0) {
    return {0, 0, false};
  }

  size_t n = s->weight_tree.size();
  if (n == 0) {
    return {0, 0, false};
  }

  int64_t accumulated_diff = 0;

  for (size_t ntry = 0; ntry < n; ++ntry) {
    if (total_weight <= 0) {
      break;
    }
    int64_t dice = FastRandLessThan(total_weight);
    size_t index = 0;

    int max_steps = 10000;
    bool retry_outer = false;
    while (index < n && --max_steps > 0) {
      int64_t left =
          s->weight_tree[index].left_weight->load(std::memory_order_relaxed);
      int64_t self = s->weight_tree[index].weight->volatile_value();

      if (dice < left) {
        index = index * 2 + 1;
      } else if (dice >= left + self) {
        dice -= (left + self);
        index = index * 2 + 2;
      } else {
        // 命中节点，尝试 AddInflight
        Weight::AddInflightResult r =
            s->weight_tree[index].weight->AddInflight(begin_time_us, index,
                                                      dice - left);
        if (r.weight_diff != 0) {
          s->UpdateParentWeights(r.weight_diff, index);
          accumulated_diff += r.weight_diff;
          total_weight += r.weight_diff;
        }
        if (r.chosen) {
          return {s->weight_tree[index].server_id, accumulated_diff, true};
        }
        retry_outer = true;
        break;
      }
    }
    if (!retry_outer && max_steps <= 0) {
      break;
    }
  }

  // 兜底：随机选一个
  size_t idx = FastRandLessThan(n);
  Weight::AddInflightResult r =
      s->weight_tree[idx].weight->AddInflight(begin_time_us, idx, 0);
  if (r.weight_diff != 0) {
    s->UpdateParentWeights(r.weight_diff, idx);
    accumulated_diff += r.weight_diff;
  }
  if (r.chosen) {
    return {s->weight_tree[idx].server_id, accumulated_diff, true};
  }
  return {0, accumulated_diff, false};
}

// ============================================================
// Feedback: 反馈 RPC 结果
//
// 读路径：通过 DoublyBufferedData::Read 获取前台快照。
// Weight::Update 内部有自己的 mutex，保护统计数据。
// diff 通过 atomic fetch_add 更新到 left_weight。
// ============================================================
int64_t WeightTree::Feedback(uint64_t server_id, int64_t latency_us,
                             int64_t begin_time_us, bool error,
                             int64_t timeout_ms) {
  DoublyBufferedData<Servers>::ScopedPtr s;
  if (db_servers_.Read(&s) != 0) {
    return 0;
  }
  std::unordered_map<uint64_t, size_t>::const_iterator it =
      s->server_map.find(server_id);
  if (it == s->server_map.end()) {
    return 0;
  }
  size_t index = it->second;
  int64_t diff = s->weight_tree[index].weight->Update(
      begin_time_us, error, timeout_ms, index);
  if (diff != 0) {
    s->UpdateParentWeights(diff, index);
  }
  return diff;
}

size_t WeightTree::Size() const {
  DoublyBufferedData<Servers>::ScopedPtr s;
  if (const_cast<DoublyBufferedData<Servers>&>(db_servers_).Read(&s) != 0) {
    return 0;
  }
  return s->weight_tree.size();
}

}  // namespace lalb
