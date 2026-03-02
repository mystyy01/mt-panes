#pragma once
#include <stdexcept>
#define MAX_WINDOWS 3
typedef struct Vector2{
    int x;
    int y;
} Vector2;

enum class border_style{
    ROUNDED,
    SQUARE
};

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


class Renderer{
public:
    Renderer();
    void draw_terminal(Vector2 pos, Vector2 size, border_style style, int term_id);
    void render();
private:
    Vector2 screen_size;
};