#pragma once

#include "types.hpp"
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#define MAX_WINDOWS 3 // temp

enum class border_style { ROUNDED, SQUARE };

class Pane {
public:
  int term_id;
  int pane_id;
  Pane(int term_id) {
    if (term_id < 0) {
      throw std::runtime_error("Terminal ID invalid when constructing pane");
    }
  }
};

class Renderer {
public:
  Renderer();
  ~Renderer();
  void draw_terminal(Vector2 pos, Vector2 size, border_style style,
                     int term_id);
  void render(const std::vector<PaneLayout> &layout,
              const std::unordered_map<int, std::string> &terminal_buffers);

private:
  int screenx, screeny;
  void draw_rect(Rect rect);
  void draw_text(Rect rect, const std::string &text);
};
