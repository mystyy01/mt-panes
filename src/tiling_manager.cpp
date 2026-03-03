#include "tiling_manager.hpp"

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

std::vector<std::unique_ptr<Node>> TilingManager::get_nodes() { return nodes; }
