#pragma once

#include "types.hpp"
#include <vector>
#include <memory>

#define D_RIGHT 0
#define D_LEFT 1
#define D_UP 2
#define D_DOWN 3

enum NodeType{
    SPLIT,
    LEAF,
    ROOT
};

class Node{
public:
    Node(int node_id, NodeType type)
        : node_id(node_id), type(type) {}
    virtual ~Node() = default;
    int node_id;
    NodeType type;
};

class LeafNode : public Node{
public:
    LeafNode(int node_id, int pane_id, int term_id, NodeType type = LEAF)
        : Node(node_id, type), pane_id(pane_id), term_id(term_id) {}
    int pane_id;
    int term_id;
};

class SplitNode : public Node{
public:
    SplitNode(
        int node_id,
        int direction,
        float ratio,
        int left_child_id,
        int right_child_id,
        NodeType type = SPLIT
    )
        : Node(node_id, type),
          direction(direction),
          ratio(ratio),
          left_child_id(left_child_id),
          right_child_id(right_child_id) {}
    int direction; // use the defines at the top of the file to specify direction
    float ratio;
    int left_child_id;
    int right_child_id;
};

class TilingManager{
public:
    TilingManager();
    Node *new_pane(int term_id);
    int next_pane_id() const;
private:
    std::vector<std::unique_ptr<Node>> nodes;
    int focused_node_id; 
};
