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
// left_weight 管理
// ============================================================
std::atomic<int64_t>* WeightTree::AllocLeftWeight() {
  left_weights_.emplace_back(0);
  return &left_weights_.back();
}

void WeightTree::FreeLastLeftWeight() { left_weights_.pop_back(); }

// ============================================================
// WeightTree 实现
// ============================================================

bool WeightTree::AddServer(uint64_t server_id, int64_t initial_weight) {
  std::unique_lock<std::shared_mutex> lock(mutex_);
  if (id_to_index_.count(server_id)) {
    return false;
  }

  if (!nodes_.empty() && initial_weight <= 0) {
    int64_t total = 0;
    for (TreeNode& node : nodes_) {
      total += node.weight->volatile_value();
    }
    initial_weight = total / static_cast<int64_t>(nodes_.size());
  }
  if (initial_weight <= 0) {
    initial_weight = Weight::kWeightScale;
  }

  size_t index = nodes_.size();
  id_to_index_[server_id] = index;

  TreeNode node;
  node.server_id = server_id;
  node.left_weight = AllocLeftWeight();
  node.weight = std::make_shared<Weight>(initial_weight);
  nodes_.push_back(node);

  int64_t w = nodes_[index].weight->volatile_value();
  if (w > 0) {
    UpdateParentWeights(w, index);
  }

  return true;
}

bool WeightTree::RemoveServer(uint64_t server_id) {
  std::unique_lock<std::shared_mutex> lock(mutex_);
  std::unordered_map<uint64_t, size_t>::iterator it =
      id_to_index_.find(server_id);
  if (it == id_to_index_.end()) {
    return false;
  }

  size_t index = it->second;
  id_to_index_.erase(it);

  int64_t rm_weight = nodes_[index].weight->Disable();
  if (rm_weight > 0) {
    UpdateParentWeights(-rm_weight, index);
  }

  size_t last = nodes_.size() - 1;
  if (index != last) {
    // 先从父节点中移除 last 节点的权值
    int64_t last_weight = nodes_[last].weight->volatile_value();
    if (last_weight > 0) {
      UpdateParentWeights(-last_weight, last);
    }

    // 移动 last 到 index（注意 left_weight 指针不跟着移动，它和位置绑定）
    nodes_[index].server_id = nodes_[last].server_id;
    nodes_[index].weight = nodes_[last].weight;
    id_to_index_[nodes_[index].server_id] = index;

    // 在新位置加上权值
    if (last_weight > 0) {
      UpdateParentWeights(last_weight, index);
    }
  }

  nodes_.pop_back();
  FreeLastLeftWeight();
  return true;
}

WeightTree::SelectResult WeightTree::Select(int64_t total_weight,
                                            int64_t begin_time_us) {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  size_t n = nodes_.size();
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
      int64_t left = nodes_[index].left_weight->load(std::memory_order_relaxed);
      int64_t self = nodes_[index].weight->volatile_value();

      if (dice < left) {
        index = index * 2 + 1;
      } else if (dice >= left + self) {
        dice -= (left + self);
        index = index * 2 + 2;
      } else {
        Weight::AddInflightResult r =
            nodes_[index].weight->AddInflight(begin_time_us, dice - left);
        if (r.weight_diff != 0) {
          UpdateParentWeights(r.weight_diff, index);
          accumulated_diff += r.weight_diff;
          total_weight += r.weight_diff;
        }
        if (r.chosen) {
          return {nodes_[index].server_id, accumulated_diff, true};
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
      nodes_[idx].weight->AddInflight(begin_time_us, 0);
  if (r.weight_diff != 0) {
    UpdateParentWeights(r.weight_diff, idx);
    accumulated_diff += r.weight_diff;
  }
  if (r.chosen) {
    return {nodes_[idx].server_id, accumulated_diff, true};
  }
  return {0, accumulated_diff, false};
}

int64_t WeightTree::Feedback(uint64_t server_id, int64_t latency_us,
                             int64_t begin_time_us, bool error,
                             int64_t timeout_ms) {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  std::unordered_map<uint64_t, size_t>::const_iterator it =
      id_to_index_.find(server_id);
  if (it == id_to_index_.end()) {
    return 0;
  }
  size_t index = it->second;
  int64_t diff = nodes_[index].weight->Update(latency_us, begin_time_us, error,
                                              timeout_ms);
  if (diff != 0) {
    UpdateParentWeights(diff, index);
  }
  return diff;
}

size_t WeightTree::Size() const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  return nodes_.size();
}

void WeightTree::UpdateParentWeights(int64_t diff, size_t index) const {
  while (index != 0) {
    size_t parent = (index - 1) >> 1;
    if ((parent << 1) + 1 == index) {
      // index 是左子节点
      nodes_[parent].left_weight->fetch_add(diff, std::memory_order_relaxed);
    }
    index = parent;
  }
}

}  // namespace lalb
