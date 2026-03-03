#include "tiling_manager.hpp"
#include "types.hpp"

TilingManager::TilingManager() : focused_node_id(-1) {}

int TilingManager::next_pane_id() const {
  return static_cast<int>(nodes.size());
}

Node *TilingManager::new_pane(int term_id) {
  const int pane_id = next_pane_id();
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

  if (leaves.size() == 2) {
    const int left_w = screen.w / 2;
    const int right_w = screen.w - left_w;
    Rect left_rect{.x = screen.x, .y = screen.y, .w = left_w, .h = screen.h};
    Rect right_rect{
        .x = screen.x + left_w, .y = screen.y, .w = right_w, .h = screen.h};

    LeafNode *left = leaves[0];
    LeafNode *right = leaves[1];

    layouts.push_back(PaneLayout{
        .pane_id = left->pane_id,
        .term_id = left->term_id,
        .rect = left_rect,
        .focused = (left->node_id == focused_node_id),
    });
    layouts.push_back(PaneLayout{
        .pane_id = right->pane_id,
        .term_id = right->term_id,
        .rect = right_rect,
        .focused = (right->node_id == focused_node_id),
    });
    return layouts;
  }

  // Temporary fallback: evenly divide width into vertical columns.
  const int cols = static_cast<int>(leaves.size());
  int x = screen.x;
  for (int i = 0; i < cols; ++i) {
    const int remaining = screen.x + screen.w - x;
    const int remaining_cols = cols - i;
    const int col_w = remaining / remaining_cols;
    Rect rect{.x = x, .y = screen.y, .w = col_w, .h = screen.h};
    LeafNode *leaf = leaves[static_cast<size_t>(i)];
    layouts.push_back(PaneLayout{
        .pane_id = leaf->pane_id,
        .term_id = leaf->term_id,
        .rect = rect,
        .focused = (leaf->node_id == focused_node_id),
    });
    x += col_w;
  }

  return layouts;
}
