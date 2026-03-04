#include "types.hpp"
#include <algorithm>
#include <clocale>
#include <cstdlib>
#include <curses.h>
#include <string>
#include <unordered_map>

#include "renderer.hpp"
#include "terminal.hpp"
#include "tiling_manager.hpp"

namespace {
constexpr int kPrefixKey = 1; // Ctrl-A

std::string key_to_input_bytes(int ch) {
  if (ch == ERR) {
    return {};
  }
  switch (ch) {
  case '\r':
  case '\n': // KEY_ENTER may come through as '\n' on some terminals
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
  case KEY_BTAB:
    return "\x1b[Z";
  default:
    if (ch >= 0 && ch <= 255) {
      return std::string(1, static_cast<char>(ch));
    }
    return {};
  }
}
} // namespace

int main() {
  std::setlocale(LC_ALL, "");

  if (const char *outer_term = std::getenv("TERM");
      outer_term != nullptr && outer_term[0] != '\0') {
    setenv("MTPANES_OUTER_TERM", outer_term, 1);
  }

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

  if (!new_pane_with_terminal()) {
    return -1;
  }

  int h, w;
  getmaxyx(stdscr, h, w);
  Rect screen_rect{.x = 0, .y = 0, .w = w, .h = h};
  std::vector<PaneLayout> layouts = tile_m.compute_layout(screen_rect);

  nodelay(stdscr, TRUE);
  keypad(stdscr, FALSE);
  set_escdelay(25);

  auto send_to_focused = [&](const std::string &bytes) {
    if (bytes.empty()) {
      return;
    }
    const int focused_term_id = tile_m.get_focused_term_id();
    Terminal *term = term_m.get_term(focused_term_id);
    if (term != nullptr) {
      term->send_cmd(bytes);
    }
  };

  bool prefix_pending = false;
  bool running = true;
  while (running) {
    const int ch = getch();

    if (ch != ERR) {
      if (prefix_pending) {
        prefix_pending = false;
        if (ch == kPrefixKey) {
          send_to_focused(std::string(1, static_cast<char>(kPrefixKey)));
        } else if (ch == 'n') {
          (void)new_pane_with_terminal();
        } else if (ch == 'c') {
          (void)close_focused();
          if (tile_m.get_nodes().empty()) {
            running = false;
          }
        } else if (ch == 'h') {
          (void)tile_m.focus_prev();
        } else if (ch == 'l' || ch == '\t') {
          (void)tile_m.focus_next();
        } else if (ch == 'q') {
          running = false;
        }
      } else if (ch == kPrefixKey) {
        prefix_pending = true;
      } else {
        const std::string input = key_to_input_bytes(ch);
        send_to_focused(input);
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
      Terminal *term = term_m.get_term(layout.term_id);
      if (term) {
        term->set_size(inner_w, inner_h);
      }
    }

    const std::vector<int> exited_terms = term_m.collect_exited_terminals();
    for (int term_id : exited_terms) {
      (void)tile_m.close_pane_by_term_id(term_id);
      emulators.erase(term_id);
    }
    if (tile_m.get_nodes().empty()) {
      running = false;
    }

    std::vector<Terminal> *terms = term_m.get_all_terminals();
    for (Terminal &term : *terms) {
      std::string out = term.read_available();
      if (out.empty()) {
        continue;
      }
      auto &emu = emulators[term.term_id];
      emu.feed(out);
      std::string reply = emu.take_response();
      if (!reply.empty()) {
        term.send_cmd(reply);
      }
    }

    r.render(layouts, emulators, true);
    if (h > 0 && w > 0) {
      attrset(A_REVERSE);
      mvhline(h - 1, 0, ' ', w);
      const std::string line = prefix_pending
                                   ? "PREFIX (Ctrl-a): n new | c close | h/l "
                                     "move | q quit | Ctrl-a send Ctrl-a"
                                   : "-- INSERT --";
      mvaddnstr(h - 1, 0, line.c_str(), w);
      attrset(A_NORMAL);
      refresh();
    }
    napms(16);
  }

  return 0;
}
