#pragma once

#include "types.hpp"
#include <memory>
#include <vector>

#define D_RIGHT 0
#define D_LEFT 1
#define D_UP 2
#define D_DOWN 3

class TilingManager {
public:
  TilingManager();
  Node *new_pane(int term_id);
  int next_pane_id() const;
  std::vector<std::unique_ptr<Node>> get_nodes();

private:
  std::vector<std::unique_ptr<Node>> nodes;
  int focused_node_id;
};
