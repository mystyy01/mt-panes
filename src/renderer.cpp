#include "renderer.hpp"
#include "types.hpp"
#include <curses.h>
#include <cctype>
#include <string>
#include <unordered_map>
#include <vector>

void Renderer::draw_rect(Rect rect) {
  // rect: x,y,w,h
  int x = rect.x, y = rect.y, w = rect.w, h = rect.h;
  if (w < 2 || h < 2) {
    return;
  }

  // corners
  mvaddch(y, x, ACS_ULCORNER);
  mvaddch(y, x + w - 1, ACS_URCORNER);
  mvaddch(y + h - 1, x, ACS_LLCORNER);
  mvaddch(y + h - 1, x + w - 1, ACS_LRCORNER);

  // horizontal edges
  for (int cx = x + 1; cx < x + w - 1; ++cx) {
    mvaddch(y, cx, ACS_HLINE);
    mvaddch(y + h - 1, cx, ACS_HLINE);
  }

  // vertical edges
  for (int cy = y + 1; cy < y + h - 1; ++cy) {
    mvaddch(cy, x, ACS_VLINE);
    mvaddch(cy, x + w - 1, ACS_VLINE);
  }
}

Renderer::Renderer() {
  initscr();
  cbreak();
  noecho();
  getmaxyx(stdscr, screeny, screenx);
}
Renderer::~Renderer() { endwin(); }

void Renderer::draw_terminal(Vector2 pos, Vector2 size, border_style style,
                             int term_id) {
  (void)pos;
  (void)size;
  (void)style;
  (void)term_id;
}
void Renderer::draw_text(Rect rect, const std::string &text) {
  const int x = rect.x + 1;
  const int y = rect.y + 1;
  const int w = rect.w - 2;
  const int h = rect.h - 2;
  if (w <= 0 || h <= 0) {
    return;
  }

  std::string cleaned;
  cleaned.reserve(text.size());
  enum class EscState { NORMAL, ESC, CSI };
  EscState state = EscState::NORMAL;
  for (unsigned char c : text) {
    if (state == EscState::NORMAL) {
      if (c == '\x1b') {
        state = EscState::ESC;
      } else if (c == '\r') {
        continue;
      } else if (c == '\n' || c == '\t' || std::isprint(c)) {
        cleaned.push_back(static_cast<char>(c));
      }
      continue;
    }
    if (state == EscState::ESC) {
      state = (c == '[') ? EscState::CSI : EscState::NORMAL;
      continue;
    }
    if (c >= 0x40 && c <= 0x7e) {
      state = EscState::NORMAL;
    }
  }

  std::vector<std::string> rows(1);
  rows.reserve(256);
  for (char c : cleaned) {
    if (c == '\n') {
      rows.emplace_back();
      continue;
    }
    if (c == '\t') {
      for (int i = 0; i < 4; ++i) {
        if (static_cast<int>(rows.back().size()) >= w) {
          rows.emplace_back();
        }
        rows.back().push_back(' ');
      }
      continue;
    }
    if (static_cast<int>(rows.back().size()) >= w) {
      rows.emplace_back();
    }
    rows.back().push_back(c);
  }

  int start = 0;
  if (static_cast<int>(rows.size()) > h) {
    start = static_cast<int>(rows.size()) - h;
  }
  int draw_row = 0;
  for (int i = start; i < static_cast<int>(rows.size()) && draw_row < h; ++i) {
    mvaddnstr(y + draw_row, x, rows[static_cast<size_t>(i)].c_str(), w);
    ++draw_row;
  }
}
void Renderer::render(
    const std::vector<PaneLayout> &layouts,
    const std::unordered_map<int, std::string> &terminal_buffers) {
  erase();
  for (const auto &layout : layouts) {
    draw_rect(layout.rect);
    auto it = terminal_buffers.find(layout.term_id);
    if (it != terminal_buffers.end()) {
      draw_text(layout.rect, it->second);
    }
  }
  refresh();
}

// draw terminal
// takes pos, border style, size, terminal object/id
// redraw every frame/every keypress/update to the read(master_fd) or write()

// seperate tiling manager - give it current pos and size of currently opened
// terminals and return a pos, size of where a new terminal would be use return
// values in draw_terminal();
