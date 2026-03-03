#include "types.hpp"
#include <algorithm>
#include <cctype>
#include <curses.h>
#include <string>
#include <unordered_map>

#include "renderer.hpp"
#include "terminal.hpp"
#include "tiling_manager.hpp"

namespace {
enum class UiMode { NORMAL, INSERT, COMMAND };

std::string key_to_input_bytes(int ch) {
  if (ch == ERR || ch == KEY_BTAB || ch == '\t') {
    return {};
  }
  if (ch > 0 && ch < 27 && ch != 27) {
    return std::string(1, static_cast<char>(ch));
  }
  switch (ch) {
  case '\n':
  case KEY_ENTER:
    return "\r";
  case KEY_BACKSPACE:
  case '\b':
  case 127:
    return "\x7f";
  case KEY_LEFT:
    return "\x1b[D";
  case KEY_RIGHT:
    return "\x1b[C";
  case KEY_UP:
    return "\x1b[A";
  case KEY_DOWN:
    return "\x1b[B";
  case KEY_HOME:
    return "\x1b[H";
  case KEY_END:
    return "\x1b[F";
  case KEY_DC:
    return "\x1b[3~";
  case KEY_NPAGE:
    return "\x1b[6~";
  case KEY_PPAGE:
    return "\x1b[5~";
  default:
    if (ch >= 0 && ch <= 255 && std::isprint(ch)) {
      return std::string(1, static_cast<char>(ch));
    }
    return {};
  }
}

bool is_backspace(int ch) {
  return ch == KEY_BACKSPACE || ch == 127 || ch == '\b';
}
} // namespace

int main() {
  Renderer r;
  TilingManager tile_m;
  TerminalManager term_m;

  std::unordered_map<int, TerminalEmulator> emulators;
  auto new_pane_with_terminal = [&]() -> bool {
    const int term_id = term_m.new_terminal();
    if (term_id < 0) {
      return false;
    }
    tile_m.new_pane(term_id);
    emulators.try_emplace(term_id);
    return true;
  };
  auto close_focused = [&]() -> bool {
    const int term_id = tile_m.close_focused_pane();
    if (term_id < 0) {
      return false;
    }
    (void)term_m.close_terminal(term_id);
    emulators.erase(term_id);
    return true;
  };

  if (!new_pane_with_terminal() || !new_pane_with_terminal()) {
    return -1;
  }

  int h, w;
  getmaxyx(stdscr, h, w);
  Rect screen_rect{.x = 0, .y = 0, .w = w, .h = h};
  std::vector<PaneLayout> layouts = tile_m.compute_layout(screen_rect);

  nodelay(stdscr, TRUE);
  keypad(stdscr, TRUE);

  UiMode mode = UiMode::INSERT;
  std::string command = ":";
  std::string status = "-- INSERT --";
  bool running = true;
  while (running) {
    const int ch = getch();

    if (ch == '\t') {
      (void)tile_m.focus_next();
    } else if (ch == KEY_BTAB) {
      (void)tile_m.focus_prev();
    } else if (mode == UiMode::COMMAND) {
      if (ch == 27) {
        mode = UiMode::NORMAL;
        command = ":";
        status = "-- NORMAL --";
      } else if (ch == '\n' || ch == KEY_ENTER) {
        if (command == ":q") {
          running = false;
        } else if (command == ":new") {
          status = new_pane_with_terminal() ? "new pane" : "new pane failed";
        } else if (command == ":close") {
          status = close_focused() ? "closed pane" : "no pane to close";
          if (tile_m.get_nodes().empty()) {
            running = false;
          }
        } else {
          status = "unknown command";
        }
        mode = UiMode::NORMAL;
        command = ":";
      } else if (is_backspace(ch)) {
        if (command.size() > 1) {
          command.pop_back();
        }
      } else if (ch >= 0 && ch <= 255 && std::isprint(ch)) {
        command.push_back(static_cast<char>(ch));
      }
    } else if (mode == UiMode::INSERT) {
      if (ch == 27) { // Ctrl+[ / ESC
        mode = UiMode::NORMAL;
        status = "-- NORMAL --";
      } else {
        const std::string input = key_to_input_bytes(ch);
        if (!input.empty()) {
          const int focused_term_id = tile_m.get_focused_term_id();
          Terminal *term = term_m.get_term(focused_term_id);
          if (term != nullptr) {
            term->send_cmd(input);
          }
        }
      }
    } else { // NORMAL
      if (ch == 'i' || ch == 'a') {
        mode = UiMode::INSERT;
        status = "-- INSERT --";
      } else if (ch == ':') {
        mode = UiMode::COMMAND;
        command = ":";
      } else if (ch == 'n') {
        status = new_pane_with_terminal() ? "new pane" : "new pane failed";
      } else if (ch == 'c') {
        status = close_focused() ? "closed pane" : "no pane to close";
        if (tile_m.get_nodes().empty()) {
          running = false;
        }
      } else if (ch == 'h') {
        (void)tile_m.focus_prev();
      } else if (ch == 'l') {
        (void)tile_m.focus_next();
      }
    }

    int next_h, next_w;
    getmaxyx(stdscr, next_h, next_w);
    if (next_h != h || next_w != w) {
      h = next_h;
      w = next_w;
      screen_rect = Rect{.x = 0, .y = 0, .w = w, .h = h};
    }
    layouts = tile_m.compute_layout(screen_rect);
    for (const auto &layout : layouts) {
      const int inner_w = std::max(1, layout.rect.w - 2);
      const int inner_h = std::max(1, layout.rect.h - 2);
      emulators[layout.term_id].resize(inner_w, inner_h);
    }

    std::vector<Terminal> *terms = term_m.get_all_terminals();
    for (Terminal &term : *terms) {
      std::string out = term.read_available();
      if (out.empty()) {
        continue;
      }
      emulators[term.term_id].feed(out);
    }

    r.render(layouts, emulators, mode == UiMode::INSERT);
    if (h > 0 && w > 0) {
      attrset(A_REVERSE);
      mvhline(h - 1, 0, ' ', w);
      std::string line;
      if (mode == UiMode::COMMAND) {
        line = command;
      } else {
        line = status;
      }
      mvaddnstr(h - 1, 0, line.c_str(), w);
      attrset(A_NORMAL);
      refresh();
    }
    napms(16);
  }

  return 0;
}
