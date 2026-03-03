#pragma once

#include "terminal_emulator.hpp"
#include "types.hpp"
#include <cstdint>
#include <stdexcept>
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
              const std::unordered_map<int, TerminalEmulator> &emulators,
              bool show_insert_cursor);

private:
  int screenx, screeny;
  void draw_rect(Rect rect, bool focused);
  void draw_emulator(Rect rect, const TerminalEmulator &emulator,
                     bool show_cursor);
  short ensure_color_pair(int fg, int bg);
  std::unordered_map<int64_t, short> color_pairs;
  short next_color_pair;
};
