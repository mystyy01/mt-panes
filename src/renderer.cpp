#include "renderer.hpp"
#include "types.hpp"
#include <cstdint>
#include <curses.h>
#include <unordered_map>
#include <vector>

namespace {
short map_color_idx(int idx) {
  static const short kMap[8] = {COLOR_BLACK, COLOR_RED,   COLOR_GREEN,
                                COLOR_YELLOW, COLOR_BLUE,  COLOR_MAGENTA,
                                COLOR_CYAN,   COLOR_WHITE};
  if (idx < 0 || idx > 7) {
    return -1;
  }
  return kMap[idx];
}
} // namespace

Renderer::Renderer() : next_color_pair(1) {
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  curs_set(0);
  getmaxyx(stdscr, screeny, screenx);
  if (has_colors()) {
    start_color();
    use_default_colors();
  }
}

Renderer::~Renderer() { endwin(); }

short Renderer::ensure_color_pair(int fg, int bg) {
  if (!has_colors()) {
    return 0;
  }
  const int64_t key =
      (static_cast<int64_t>(fg + 2) << 32) | static_cast<uint32_t>(bg + 2);
  const auto it = color_pairs.find(key);
  if (it != color_pairs.end()) {
    return it->second;
  }
  if (next_color_pair >= COLOR_PAIRS) {
    return 0;
  }
  const short id = next_color_pair++;
  if (init_pair(id, static_cast<short>(fg), static_cast<short>(bg)) == ERR) {
    return 0;
  }
  color_pairs[key] = id;
  return id;
}

void Renderer::draw_rect(Rect rect, bool focused) {
  const int x = rect.x;
  const int y = rect.y;
  const int w = rect.w;
  const int h = rect.h;
  if (w < 2 || h < 2) {
    return;
  }

  attrset(A_NORMAL);
  const short border_pair =
      focused ? ensure_color_pair(COLOR_CYAN, -1) : ensure_color_pair(-1, -1);
  if (border_pair > 0) {
    attron(COLOR_PAIR(border_pair));
  }
  if (focused) {
    attron(A_BOLD);
  }

  mvaddch(y, x, ACS_ULCORNER);
  mvaddch(y, x + w - 1, ACS_URCORNER);
  mvaddch(y + h - 1, x, ACS_LLCORNER);
  mvaddch(y + h - 1, x + w - 1, ACS_LRCORNER);

  for (int cx = x + 1; cx < x + w - 1; ++cx) {
    mvaddch(y, cx, ACS_HLINE);
    mvaddch(y + h - 1, cx, ACS_HLINE);
  }
  for (int cy = y + 1; cy < y + h - 1; ++cy) {
    mvaddch(cy, x, ACS_VLINE);
    mvaddch(cy, x + w - 1, ACS_VLINE);
  }
  attrset(A_NORMAL);
}

void Renderer::draw_emulator(Rect rect, const TerminalEmulator &emulator,
                             bool show_cursor) {
  const int x = rect.x + 1;
  const int y = rect.y + 1;
  const int w = rect.w - 2;
  const int h = rect.h - 2;
  if (w <= 0 || h <= 0) {
    return;
  }

  const int rows = std::min(h, emulator.height());
  const int cols = std::min(w, emulator.width());

  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < cols; ++c) {
      const TerminalCell &cell = emulator.cell(r, c);
      attrset(A_NORMAL);
      const short fg = map_color_idx(cell.fg);
      const short bg = map_color_idx(cell.bg);
      const short pair = ensure_color_pair(fg, bg);
      if (pair > 0) {
        attron(COLOR_PAIR(pair));
      }
      if (cell.bold) {
        attron(A_BOLD);
      }
      mvaddch(y + r, x + c, cell.ch);
    }
  }

  if (show_cursor) {
    const int cr = emulator.cursor_row();
    const int cc = emulator.cursor_col();
    if (cr >= 0 && cr < rows && cc >= 0 && cc < cols) {
      const TerminalCell &cell = emulator.cell(cr, cc);
      attrset(A_REVERSE);
      mvaddch(y + cr, x + cc, cell.ch ? cell.ch : ' ');
    }
  }
  attrset(A_NORMAL);
}

void Renderer::draw_terminal(Vector2 pos, Vector2 size, border_style style,
                             int term_id) {
  (void)pos;
  (void)size;
  (void)style;
  (void)term_id;
}

void Renderer::render(const std::vector<PaneLayout> &layouts,
                      const std::unordered_map<int, TerminalEmulator> &emulators,
                      bool show_insert_cursor) {
  erase();
  for (const auto &layout : layouts) {
    draw_rect(layout.rect, layout.focused);
    auto it = emulators.find(layout.term_id);
    if (it == emulators.end()) {
      continue;
    }
    const bool pane_cursor = show_insert_cursor && layout.focused;
    draw_emulator(layout.rect, it->second, pane_cursor);
  }
  refresh();
}
