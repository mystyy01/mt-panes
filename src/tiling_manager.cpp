#include "tiling_manager.hpp"

Node TilingManager::new_pane(int term_id){
    Node return_node;
    return_node.node_id = 1;
    return_node.type = ROOT;
    return return_node;
}