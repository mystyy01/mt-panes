#pragma once

struct Vector2 {
  int x;
  int y;
};

enum NodeType { SPLIT, LEAF, ROOT };

class Node {
public:
  Node(int node_id, NodeType type) : node_id(node_id), type(type) {}
  virtual ~Node() = default;
  int node_id;
  NodeType type;
};

class LeafNode : public Node {
public:
  LeafNode(int node_id, int pane_id, int term_id, NodeType type = LEAF)
      : Node(node_id, type), pane_id(pane_id), term_id(term_id) {}
  int pane_id;
  int term_id;
};

class SplitNode : public Node {
public:
  SplitNode(int node_id, int direction, float ratio, int left_child_id,
            int right_child_id, NodeType type = SPLIT)
      : Node(node_id, type), direction(direction), ratio(ratio),
        left_child_id(left_child_id), right_child_id(right_child_id) {}
  int direction; // use the defines at the top of the file to specify direction
  float ratio;
  int left_child_id;
  int right_child_id;
};

struct Rect {
  int x, y;
  int w, h;
};

struct PaneLayout {
  int pane_id;
  int term_id;
  Rect rect;
  bool focused;
};
