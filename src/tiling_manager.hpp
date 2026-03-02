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
    virtual ~Node() = default;
    int node_id;
    NodeType type;
};

class LeafNode : public Node{
public:
    int pane_id;
};

class SplitNode : public Node{
public:
    int direction; // use the defines at the top of the file to specify direction
    float ratio;
    int left_child_id;
    int right_child_id;
};

class TilingManager{
public:
    Node new_pane(int term_id);
private:
    std::vector<std::unique_ptr<Node>> nodes;    
};