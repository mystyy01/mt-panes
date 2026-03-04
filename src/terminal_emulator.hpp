#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Color encoding:
//   -1           = default color
//   0 .. 255     = indexed (256-color palette)
//   0x01RRGGBB   = true-color RGB (bit 24 set)
using TermColor = int32_t;

inline constexpr TermColor kColorDefault = -1;

inline TermColor color_rgb(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<int32_t>(0x01000000u | (uint32_t(r) << 16) |
                              (uint32_t(g) << 8) | b);
}
inline bool color_is_rgb(TermColor c) { return c >= 0x01000000; }
inline uint8_t color_r(TermColor c) { return (c >> 16) & 0xFF; }
inline uint8_t color_g(TermColor c) { return (c >> 8) & 0xFF; }
inline uint8_t color_b(TermColor c) { return c & 0xFF; }

struct TerminalCell {
  char32_t ch = U' ';
  TermColor fg = kColorDefault;
  TermColor bg = kColorDefault;
  bool bold = false;
  bool dim = false;
  bool italic = false;
  bool underline = false;
  bool reverse_attr = false;
  bool strikethrough = false;
  bool wide = false;       // left half of a double-width character
  bool wide_cont = false;  // right half placeholder (skip when rendering)
};

class TerminalEmulator {
public:
  TerminalEmulator(int width = 80, int height = 24);

  void reset();
  void resize(int width, int height);
  void feed(std::string_view bytes);

  int width() const;
  int height() const;
  int cursor_row() const;
  int cursor_col() const;
  const TerminalCell &cell(int row, int col) const;

  // Returns and clears any pending response bytes (e.g. DSR replies)
  std::string take_response();

private:
  enum class ParseState { NORMAL, ESC, CSI, OSC, OSC_ESC };

  int width_;
  int height_;
  int cursor_row_;
  int cursor_col_;
  int saved_row_;
  int saved_col_;
  bool wrap_pending_;

  TermColor fg_;
  TermColor bg_;
  bool bold_;
  bool dim_;
  bool italic_;
  bool underline_;
  bool reverse_attr_;
  bool strikethrough_;

  int scroll_top_;
  int scroll_bottom_;

  ParseState state_;
  std::string csi_buf_;
  std::string osc_buf_;
  int skip_bytes_;

  // UTF-8 decode state
  char32_t utf8_cp_;
  int utf8_remaining_;

  std::vector<TerminalCell> cells_;
  std::string response_buf_;

  TerminalCell blank_cell() const;
  TerminalCell &cell_mut(int row, int col);
  void clamp_cursor();
  void clear_all();
  void clear_line(int row);
  void scroll_up_region(int top, int bottom, int lines);
  void scroll_down_region(int top, int bottom, int lines);
  void newline();
  void reverse_index();
  void carriage_return();
  void backspace();
  void tab();
  void erase_wide_char(int row, int col);
  void put_codepoint(char32_t cp);
  void handle_csi(char final_char);
  void handle_sgr(std::string_view params_str);
  void handle_osc();
};
