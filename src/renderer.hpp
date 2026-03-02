#pragma once
#include <stdexcept>
#define MAX_WINDOWS 3

class Pane{
public:
    int term_id;
    int pane_id;
    Pane(int term_id){
        if (term_id < 0){
            throw std::runtime_error("Terminal ID invalid when constructing pane");
        }
    }
};

void render();