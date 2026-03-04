#include "tiling_manager.hpp"
#include "types.hpp"
#include <algorithm>
#include <functional>

TilingManager::TilingManager() : next_id(0), focused_node_id(-1) {}

int TilingManager::next_pane_id() const {
  return next_id;
}

int TilingManager::get_focused_term_id() const {
  for (const auto &node : nodes) {
    if ((node->type == LEAF || node->type == ROOT) &&
        node->node_id == focused_node_id) {
      const auto *leaf = static_cast<const LeafNode *>(node.get());
      return leaf->term_id;
    }
  }
  return -1;
}

bool TilingManager::focus_next() {
  std::vector<const LeafNode *> leaves;
  leaves.reserve(nodes.size());
  for (const auto &node : nodes) {
    if (node->type == LEAF || node->type == ROOT) {
      leaves.push_back(static_cast<const LeafNode *>(node.get()));
    }
  }
  if (leaves.empty()) {
    return false;
  }
  size_t current = 0;
  for (size_t i = 0; i < leaves.size(); ++i) {
    if (leaves[i]->node_id == focused_node_id) {
      current = i;
      break;
    }
  }
  const size_t next = (current + 1) % leaves.size();
  focused_node_id = leaves[next]->node_id;
  return true;
}

bool TilingManager::focus_prev() {
  std::vector<const LeafNode *> leaves;
  leaves.reserve(nodes.size());
  for (const auto &node : nodes) {
    if (node->type == LEAF || node->type == ROOT) {
      leaves.push_back(static_cast<const LeafNode *>(node.get()));
    }
  }
  if (leaves.empty()) {
    return false;
  }
  size_t current = 0;
  for (size_t i = 0; i < leaves.size(); ++i) {
    if (leaves[i]->node_id == focused_node_id) {
      current = i;
      break;
    }
  }
  const size_t prev = (current + leaves.size() - 1) % leaves.size();
  focused_node_id = leaves[prev]->node_id;
  return true;
}

int TilingManager::close_focused_pane() {
  for (auto it = nodes.begin(); it != nodes.end(); ++it) {
    Node *node = it->get();
    if ((node->type != LEAF && node->type != ROOT) ||
        node->node_id != focused_node_id) {
      continue;
    }
    const int term_id = static_cast<LeafNode *>(node)->term_id;
    nodes.erase(it);

    focused_node_id = -1;
    for (const auto &remaining : nodes) {
      if (remaining->type == LEAF || remaining->type == ROOT) {
        focused_node_id = remaining->node_id;
        break;
      }
    }
    if (nodes.size() == 1 && nodes[0]->type == LEAF) {
      nodes[0]->type = ROOT;
    }
    return term_id;
  }
  return -1;
}

bool TilingManager::close_pane_by_term_id(int term_id) {
  for (auto it = nodes.begin(); it != nodes.end(); ++it) {
    Node *node = it->get();
    if (node->type != LEAF && node->type != ROOT) {
      continue;
    }
    if (static_cast<LeafNode *>(node)->term_id != term_id) {
      continue;
    }

    const bool was_focused = (node->node_id == focused_node_id);
    nodes.erase(it);

    if (nodes.empty()) {
      focused_node_id = -1;
      return true;
    }
    if (nodes.size() == 1 && nodes[0]->type == LEAF) {
      nodes[0]->type = ROOT;
    }
    if (was_focused) {
      focused_node_id = -1;
      for (const auto &remaining : nodes) {
        if (remaining->type == LEAF || remaining->type == ROOT) {
          focused_node_id = remaining->node_id;
          break;
        }
      }
    }
    return true;
  }
  return false;
}

Node *TilingManager::new_pane(int term_id) {
  const int pane_id = next_pane_id();
  ++next_id;
  const NodeType node_type = nodes.empty() ? ROOT : LEAF;

  auto new_leaf =
      std::make_unique<LeafNode>(pane_id, pane_id, term_id, node_type);
  Node *new_node = new_leaf.get();

  nodes.push_back(std::move(new_leaf));
  focused_node_id = new_node->node_id;
  return new_node;
}

const std::vector<std::unique_ptr<Node>> &TilingManager::get_nodes() const {
  return nodes;
}

std::vector<PaneLayout> TilingManager::compute_layout(Rect screen) {
  std::vector<PaneLayout> layouts;
  std::vector<LeafNode *> leaves;

  for (auto &node : nodes) {
    if (node->type == LEAF || node->type == ROOT) {
      leaves.push_back(static_cast<LeafNode *>(node.get()));
    }
  }

  if (leaves.empty()) {
    return layouts;
  }

  if (leaves.size() == 1) {
    LeafNode *leaf = leaves[0];
    layouts.push_back(PaneLayout{
        .pane_id = leaf->pane_id,
        .term_id = leaf->term_id,
        .rect = screen,
        .focused = (leaf->node_id == focused_node_id),
    });
    return layouts;
  }

  std::function<void(size_t, size_t, Rect)> layout_range =
      [&](size_t begin, size_t end, Rect rect) {
        const size_t count = end - begin;
        if (count == 0) {
          return;
        }
        if (count == 1) {
          LeafNode *leaf = leaves[begin];
          layouts.push_back(PaneLayout{
              .pane_id = leaf->pane_id,
              .term_id = leaf->term_id,
              .rect = rect,
              .focused = (leaf->node_id == focused_node_id),
          });
          return;
        }

        bool split_vertical = rect.w >= rect.h;
        if (split_vertical && rect.w < 2) {
          split_vertical = false;
        } else if (!split_vertical && rect.h < 2) {
          split_vertical = true;
        }

        const size_t left_count = count / 2;
        if (split_vertical) {
          int left_w = static_cast<int>((static_cast<long long>(rect.w) *
                                         static_cast<long long>(left_count)) /
                                        static_cast<long long>(count));
          left_w = std::max(1, std::min(left_w, rect.w - 1));
          Rect left{.x = rect.x, .y = rect.y, .w = left_w, .h = rect.h};
          Rect right{
              .x = rect.x + left_w, .y = rect.y, .w = rect.w - left_w, .h = rect.h};
          layout_range(begin, begin + left_count, left);
          layout_range(begin + left_count, end, right);
          return;
        }

        int top_h = static_cast<int>((static_cast<long long>(rect.h) *
                                      static_cast<long long>(left_count)) /
                                     static_cast<long long>(count));
        top_h = std::max(1, std::min(top_h, rect.h - 1));
        Rect top{.x = rect.x, .y = rect.y, .w = rect.w, .h = top_h};
        Rect bottom{
            .x = rect.x, .y = rect.y + top_h, .w = rect.w, .h = rect.h - top_h};
        layout_range(begin, begin + left_count, top);
        layout_range(begin + left_count, end, bottom);
      };

  layout_range(0, leaves.size(), screen);

  return layouts;
}
