#define _XOPEN_SOURCE_EXTENDED 1
#include "renderer.hpp"
#include "types.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <curses.h>
#include <limits>
#include <string>
#include <string_view>
#include <unistd.h>
#include <unordered_map>
#include <vector>

namespace {
int pack_rgb(int r, int g, int b) {
  r = std::clamp(r, 0, 255);
  g = std::clamp(g, 0, 255);
  b = std::clamp(b, 0, 255);
  return (r << 16) | (g << 8) | b;
}

int xterm_index_to_rgb(int idx) {
  static const int basic16[16][3] = {
      {0, 0, 0},       {205, 0, 0},     {0, 205, 0},     {205, 205, 0},
      {0, 0, 238},     {205, 0, 205},   {0, 205, 205},   {229, 229, 229},
      {127, 127, 127}, {255, 0, 0},     {0, 255, 0},     {255, 255, 0},
      {92, 92, 255},   {255, 0, 255},   {0, 255, 255},   {255, 255, 255},
  };
  idx = std::clamp(idx, 0, 255);
  if (idx < 16) {
    return pack_rgb(basic16[idx][0], basic16[idx][1], basic16[idx][2]);
  }
  if (idx < 232) {
    int n = idx - 16;
    int r = n / 36;
    int g = (n / 6) % 6;
    int b = n % 6;
    auto level = [](int v) { return v == 0 ? 0 : 55 + v * 40; };
    return pack_rgb(level(r), level(g), level(b));
  }
  int gray = 8 + (idx - 232) * 10;
  return pack_rgb(gray, gray, gray);
}

int map_term_color(TermColor c, bool direct_rgb_mode) {
  if (c == kColorDefault) return -1;

  if (color_is_rgb(c)) {
    const int r = color_r(c);
    const int g = color_g(c);
    const int b = color_b(c);
    if (direct_rgb_mode) {
      return pack_rgb(r, g, b);
    }
    int idx;
    if (r == g && g == b) {
      if (r < 8)
        idx = 16;
      else if (r > 248)
        idx = 231;
      else
        idx = static_cast<short>(232 + (r - 8) * 24 / 240);
    } else {
      int ri = (r > 47) ? (r - 35) / 40 : 0;
      int gi = (g > 47) ? (g - 35) / 40 : 0;
      int bi = (b > 47) ? (b - 35) / 40 : 0;
      idx = static_cast<short>(16 + 36 * ri + 6 * gi + bi);
    }
    if (COLORS >= 256) return idx;
    // Fallback to 8-color: map the 256 index to basic
    static const int kBasic[8] = {COLOR_BLACK, COLOR_RED,     COLOR_GREEN,
                                  COLOR_YELLOW, COLOR_BLUE,   COLOR_MAGENTA,
                                  COLOR_CYAN,   COLOR_WHITE};
    if (idx < 8) return kBasic[idx];
    if (idx < 16) return kBasic[idx - 8];
    return -1;
  }

  // Indexed color (0-255)
  const int idx = static_cast<int>(c);
  if (idx < 0 || idx > 255) return -1;
  if (direct_rgb_mode) {
    return xterm_index_to_rgb(idx);
  }

  if (COLORS >= 256) {
    return idx;
  }

  // Map to basic 8 colors for limited terminals
  static const int kMap[8] = {COLOR_BLACK, COLOR_RED,     COLOR_GREEN,
                              COLOR_YELLOW, COLOR_BLUE,   COLOR_MAGENTA,
                              COLOR_CYAN,   COLOR_WHITE};
  if (idx < 8) return kMap[idx];
  if (idx < 16) return kMap[idx - 8];
  return -1;
}

bool terminfo_exists(std::string_view name) {
  if (name.empty()) {
    return false;
  }
  const std::vector<std::string> roots = {
      "/usr/share/terminfo", "/usr/lib/terminfo", "/lib/terminfo"};
  const char first = name[0];
  for (const auto &root : roots) {
    std::string p1 = root + "/" + first + "/" + std::string(name);
    std::string p2 = root + "/./" + first + "/" + std::string(name);
    if (access(p1.c_str(), R_OK) == 0 || access(p2.c_str(), R_OK) == 0) {
      return true;
    }
  }
  return false;
}
} // namespace

Renderer::Renderer()
    : next_color_pair(1), max_color_pairs(0), direct_rgb_mode(false) {
  const char *term = std::getenv("TERM");
  if (term != nullptr) {
    std::string current_term(term);
    const bool looks_xtermish =
        current_term.find("kitty") != std::string::npos ||
        current_term.find("xterm") != std::string::npos;
    const bool already_direct =
        current_term.find("-direct") != std::string::npos;
    if (looks_xtermish && !already_direct && terminfo_exists("xterm-direct")) {
      setenv("TERM", "xterm-direct", 1);
    }
  }

  initscr();
  raw();
  noecho();
  keypad(stdscr, FALSE);
  curs_set(0);
  getmaxyx(stdscr, screeny, screenx);
  if (has_colors()) {
    start_color();
    use_default_colors();
  }
  max_color_pairs = COLOR_PAIRS;
  direct_rgb_mode = (tigetflag("RGB") > 0);
}

Renderer::~Renderer() { endwin(); }

int Renderer::ensure_color_pair(int fg, int bg) {
  if (!has_colors()) {
    return 0;
  }
  const uint64_t key = (static_cast<uint64_t>(static_cast<uint32_t>(fg + 2))
                        << 32) |
                       static_cast<uint32_t>(bg + 2);
  const auto it = color_pairs.find(key);
  if (it != color_pairs.end()) {
    return it->second;
  }
  if (max_color_pairs <= 0 || next_color_pair >= max_color_pairs) {
    return 0;
  }
  const int id = next_color_pair++;
  int rc = ERR;
#if NCURSES_EXT_COLORS
  rc = init_extended_pair(id, fg, bg);
#else
  rc = init_pair(static_cast<short>(id), static_cast<short>(fg),
                 static_cast<short>(bg));
#endif
  if (rc == ERR) {
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
  int border_pair =
      focused ? ensure_color_pair(COLOR_CYAN, -1) : ensure_color_pair(-1, -1);
  if (wattr_set(stdscr, focused ? A_BOLD : A_NORMAL, 0, &border_pair) == ERR) {
    if (border_pair > 0) {
      attron(COLOR_PAIR(static_cast<short>(border_pair)));
    }
    if (focused) {
      attron(A_BOLD);
    }
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
      const TerminalCell &tc = emulator.cell(r, c);
      if (tc.wide_cont) continue; // skip right half of wide chars

      attr_t attrs = A_NORMAL;
      if (tc.bold) attrs |= A_BOLD;
      if (tc.dim) attrs |= A_DIM;
#ifdef A_ITALIC
      if (tc.italic) attrs |= A_ITALIC;
#endif
      if (tc.underline) attrs |= A_UNDERLINE;
      if (tc.reverse_attr) attrs |= A_REVERSE;

      const int fg = map_term_color(tc.fg, direct_rgb_mode);
      const int bg = map_term_color(tc.bg, direct_rgb_mode);
      const int pair = ensure_color_pair(fg, bg);

      cchar_t cc;
      wchar_t wc[2] = {static_cast<wchar_t>(tc.ch), 0};
      if (setcchar(&cc, wc, attrs, 0, &pair) == ERR) {
        const short pair_short =
            static_cast<short>(std::clamp(pair, 0, static_cast<int>(std::numeric_limits<short>::max())));
        setcchar(&cc, wc, attrs, pair_short, nullptr);
      }
      mvadd_wch(y + r, x + c, &cc);
    }
  }

  if (show_cursor) {
    const int cr = emulator.cursor_row();
    const int cc_col = emulator.cursor_col();
    if (cr >= 0 && cr < rows && cc_col >= 0 && cc_col < cols) {
      const TerminalCell &tc = emulator.cell(cr, cc_col);
      cchar_t cc;
      wchar_t wc[2] = {static_cast<wchar_t>(tc.ch ? tc.ch : U' '), 0};
      setcchar(&cc, wc, A_REVERSE, 0, nullptr);
      mvadd_wch(y + cr, x + cc_col, &cc);
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
  // Rebuild color-pair mapping each frame so long-running sessions do not
  // exhaust COLOR_PAIRS and silently fall back to pair 0.
  color_pairs.clear();
  next_color_pair = 1;

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
