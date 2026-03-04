#include "terminal_emulator.hpp"

#include <algorithm>
#include <cstdlib>
#include <wchar.h>

namespace {
int clamp_int(int v, int lo, int hi) { return std::max(lo, std::min(v, hi)); }

std::vector<int> parse_params(const std::string &s) {
  std::vector<int> out;
  if (s.empty()) {
    return out;
  }
  size_t i = 0;
  while (i <= s.size()) {
    size_t j = s.find(';', i);
    if (j == std::string::npos) {
      j = s.size();
    }
    if (j == i) {
      out.push_back(-1);
    } else {
      out.push_back(std::atoi(s.substr(i, j - i).c_str()));
    }
    if (j == s.size()) {
      break;
    }
    i = j + 1;
  }
  return out;
}

int get_param(const std::vector<int> &p, size_t idx, int def) {
  if (idx >= p.size() || p[idx] < 0) {
    return def;
  }
  return p[idx];
}

int parse_int_or_default(std::string_view s, int def = -1) {
  if (s.empty()) {
    return def;
  }
  int sign = 1;
  size_t i = 0;
  if (s[0] == '-') {
    sign = -1;
    i = 1;
    if (i >= s.size()) {
      return def;
    }
  }
  int value = 0;
  for (; i < s.size(); ++i) {
    const char ch = s[i];
    if (ch < '0' || ch > '9') {
      return def;
    }
    value = value * 10 + (ch - '0');
  }
  return sign * value;
}

std::vector<std::string_view> split_sv(std::string_view s, char delim) {
  std::vector<std::string_view> out;
  size_t start = 0;
  while (start <= s.size()) {
    size_t end = s.find(delim, start);
    if (end == std::string_view::npos) {
      end = s.size();
    }
    out.emplace_back(s.substr(start, end - start));
    if (end == s.size()) {
      break;
    }
    start = end + 1;
  }
  return out;
}

int codepoint_width(char32_t cp) {
  if (cp < 0x20) return 0;
  if (cp < 0x7f) return 1;
  if (cp == 0x7f) return 0;
  int w = wcwidth(static_cast<wchar_t>(cp));
  if (w < 0) return 1;
  return w;
}
} // namespace

TerminalEmulator::TerminalEmulator(int width, int height)
    : width_(1), height_(1), cursor_row_(0), cursor_col_(0), saved_row_(0),
      saved_col_(0), wrap_pending_(false), fg_(kColorDefault),
      bg_(kColorDefault), bold_(false), dim_(false), italic_(false),
      underline_(false), reverse_attr_(false), strikethrough_(false),
      scroll_top_(0), scroll_bottom_(0), state_(ParseState::NORMAL),
      skip_bytes_(0), utf8_cp_(0), utf8_remaining_(0) {
  resize(width, height);
}

TerminalCell TerminalEmulator::blank_cell() const {
  TerminalCell c;
  c.bg = bg_;
  return c;
}

void TerminalEmulator::reset() {
  cursor_row_ = 0;
  cursor_col_ = 0;
  saved_row_ = 0;
  saved_col_ = 0;
  wrap_pending_ = false;
  fg_ = kColorDefault;
  bg_ = kColorDefault;
  bold_ = false;
  dim_ = false;
  italic_ = false;
  underline_ = false;
  reverse_attr_ = false;
  strikethrough_ = false;
  scroll_top_ = 0;
  scroll_bottom_ = height_ - 1;
  state_ = ParseState::NORMAL;
  csi_buf_.clear();
  osc_buf_.clear();
  skip_bytes_ = 0;
  utf8_cp_ = 0;
  utf8_remaining_ = 0;
  clear_all();
}

void TerminalEmulator::resize(int width, int height) {
  width = std::max(1, width);
  height = std::max(1, height);
  if (width == width_ && height == height_ && !cells_.empty()) {
    return;
  }

  TermColor saved_bg = bg_;
  bg_ = kColorDefault;
  std::vector<TerminalCell> next(static_cast<size_t>(width * height),
                                 blank_cell());
  bg_ = saved_bg;

  if (!cells_.empty()) {
    const int copy_h = std::min(height, height_);
    const int copy_w = std::min(width, width_);
    for (int r = 0; r < copy_h; ++r) {
      for (int c = 0; c < copy_w; ++c) {
        next[static_cast<size_t>(r * width + c)] =
            cells_[static_cast<size_t>(r * width_ + c)];
      }
    }
  }
  width_ = width;
  height_ = height;
  cells_.swap(next);
  scroll_top_ = 0;
  scroll_bottom_ = height_ - 1;
  clamp_cursor();
}

int TerminalEmulator::width() const { return width_; }
int TerminalEmulator::height() const { return height_; }
int TerminalEmulator::cursor_row() const { return cursor_row_; }
int TerminalEmulator::cursor_col() const {
  return std::min(cursor_col_, width_ - 1);
}

std::string TerminalEmulator::take_response() {
  std::string out;
  out.swap(response_buf_);
  return out;
}

const TerminalCell &TerminalEmulator::cell(int row, int col) const {
  static const TerminalCell kEmpty{};
  if (row < 0 || col < 0 || row >= height_ || col >= width_ ||
      cells_.empty()) {
    return kEmpty;
  }
  return cells_[static_cast<size_t>(row * width_ + col)];
}

TerminalCell &TerminalEmulator::cell_mut(int row, int col) {
  return cells_[static_cast<size_t>(row * width_ + col)];
}

void TerminalEmulator::clamp_cursor() {
  cursor_row_ = clamp_int(cursor_row_, 0, height_ - 1);
  cursor_col_ = clamp_int(cursor_col_, 0, width_ - 1);
}

void TerminalEmulator::clear_all() {
  TermColor saved_bg = bg_;
  bg_ = kColorDefault;
  std::fill(cells_.begin(), cells_.end(), blank_cell());
  bg_ = saved_bg;
}

void TerminalEmulator::clear_line(int row) {
  if (row < 0 || row >= height_) {
    return;
  }
  for (int c = 0; c < width_; ++c) {
    cell_mut(row, c) = blank_cell();
  }
}

void TerminalEmulator::scroll_up_region(int top, int bottom, int lines) {
  if (top < 0 || bottom >= height_ || top > bottom) return;
  lines = clamp_int(lines, 0, bottom - top + 1);
  if (lines <= 0) return;

  for (int r = top; r <= bottom - lines; ++r) {
    for (int c = 0; c < width_; ++c) {
      cell_mut(r, c) = cell(r + lines, c);
    }
  }
  for (int r = bottom - lines + 1; r <= bottom; ++r) {
    clear_line(r);
  }
}

void TerminalEmulator::scroll_down_region(int top, int bottom, int lines) {
  if (top < 0 || bottom >= height_ || top > bottom) return;
  lines = clamp_int(lines, 0, bottom - top + 1);
  if (lines <= 0) return;

  for (int r = bottom; r >= top + lines; --r) {
    for (int c = 0; c < width_; ++c) {
      cell_mut(r, c) = cell(r - lines, c);
    }
  }
  for (int r = top; r < top + lines; ++r) {
    clear_line(r);
  }
}

void TerminalEmulator::newline() {
  if (cursor_row_ == scroll_bottom_) {
    scroll_up_region(scroll_top_, scroll_bottom_, 1);
  } else if (cursor_row_ < height_ - 1) {
    ++cursor_row_;
  }
}

void TerminalEmulator::reverse_index() {
  if (cursor_row_ == scroll_top_) {
    scroll_down_region(scroll_top_, scroll_bottom_, 1);
  } else if (cursor_row_ > 0) {
    --cursor_row_;
  }
}

void TerminalEmulator::carriage_return() { cursor_col_ = 0; }

void TerminalEmulator::backspace() {
  if (cursor_col_ > 0) {
    --cursor_col_;
  }
}

void TerminalEmulator::tab() {
  const int next = ((cursor_col_ / 8) + 1) * 8;
  cursor_col_ = std::min(next, width_ - 1);
}

void TerminalEmulator::erase_wide_char(int row, int col) {
  if (row < 0 || row >= height_ || col < 0 || col >= width_) return;
  TerminalCell &c = cell_mut(row, col);
  if (c.wide && col + 1 < width_) {
    TerminalCell &right = cell_mut(row, col + 1);
    right = blank_cell();
  }
  if (c.wide_cont && col > 0) {
    TerminalCell &left = cell_mut(row, col - 1);
    left.ch = U' ';
    left.wide = false;
  }
}

void TerminalEmulator::put_codepoint(char32_t cp) {
  int w = codepoint_width(cp);
  if (w == 0) return; // combining / zero-width, skip for now

  // Handle deferred wrap
  if (wrap_pending_) {
    wrap_pending_ = false;
    cursor_col_ = 0;
    newline();
  }

  // Wide char doesn't fit at end of line
  if (w == 2 && cursor_col_ + 1 >= width_) {
    if (cursor_col_ < width_) {
      erase_wide_char(cursor_row_, cursor_col_);
      cell_mut(cursor_row_, cursor_col_) = blank_cell();
    }
    cursor_col_ = 0;
    newline();
  }

  // Erase anything being overwritten
  erase_wide_char(cursor_row_, cursor_col_);
  if (w == 2 && cursor_col_ + 1 < width_) {
    erase_wide_char(cursor_row_, cursor_col_ + 1);
  }

  TerminalCell &c = cell_mut(cursor_row_, cursor_col_);
  c.ch = cp;
  c.fg = fg_;
  c.bg = bg_;
  c.bold = bold_;
  c.dim = dim_;
  c.italic = italic_;
  c.underline = underline_;
  c.reverse_attr = reverse_attr_;
  c.strikethrough = strikethrough_;
  c.wide = (w == 2);
  c.wide_cont = false;

  if (w == 2 && cursor_col_ + 1 < width_) {
    TerminalCell &cont = cell_mut(cursor_row_, cursor_col_ + 1);
    cont = blank_cell();
    cont.wide_cont = true;
  }

  cursor_col_ += w;

  if (cursor_col_ >= width_) {
    cursor_col_ = width_ - 1;
    wrap_pending_ = true;
  }
}

void TerminalEmulator::handle_sgr(std::string_view params_str) {
  if (params_str.empty()) {
    fg_ = kColorDefault;
    bg_ = kColorDefault;
    bold_ = dim_ = italic_ = underline_ = reverse_attr_ = strikethrough_ =
        false;
    return;
  }

  auto set_indexed_color = [&](bool foreground, int idx) {
    const TermColor color = clamp_int(idx, 0, 255);
    if (foreground) {
      fg_ = color;
    } else {
      bg_ = color;
    }
  };
  auto set_rgb_color = [&](bool foreground, int r, int g, int b) {
    const TermColor color =
        color_rgb(static_cast<uint8_t>(clamp_int(r, 0, 255)),
                  static_cast<uint8_t>(clamp_int(g, 0, 255)),
                  static_cast<uint8_t>(clamp_int(b, 0, 255)));
    if (foreground) {
      fg_ = color;
    } else {
      bg_ = color;
    }
  };
  auto apply_basic = [&](int code) {
    code = (code < 0) ? 0 : code;
    if (code == 0) {
      fg_ = kColorDefault;
      bg_ = kColorDefault;
      bold_ = dim_ = italic_ = underline_ = reverse_attr_ = strikethrough_ =
          false;
    } else if (code == 1) {
      bold_ = true;
    } else if (code == 2) {
      dim_ = true;
    } else if (code == 3) {
      italic_ = true;
    } else if (code == 4) {
      underline_ = true;
    } else if (code == 7) {
      reverse_attr_ = true;
    } else if (code == 9) {
      strikethrough_ = true;
    } else if (code == 21 || code == 22) {
      bold_ = false;
      dim_ = false;
    } else if (code == 23) {
      italic_ = false;
    } else if (code == 24) {
      underline_ = false;
    } else if (code == 27) {
      reverse_attr_ = false;
    } else if (code == 29) {
      strikethrough_ = false;
    } else if (code >= 30 && code <= 37) {
      fg_ = code - 30;
    } else if (code == 39) {
      fg_ = kColorDefault;
    } else if (code >= 40 && code <= 47) {
      bg_ = code - 40;
    } else if (code == 49) {
      bg_ = kColorDefault;
    } else if (code >= 90 && code <= 97) {
      fg_ = code - 90 + 8;
    } else if (code >= 100 && code <= 107) {
      bg_ = code - 100 + 8;
    }
  };
  auto apply_colon_extended = [&](std::string_view token) {
    const std::vector<std::string_view> parts = split_sv(token, ':');
    if (parts.empty()) {
      return;
    }
    const int code = parse_int_or_default(parts[0], 0);
    if (code != 38 && code != 48) {
      apply_basic(code);
      return;
    }
    if (parts.size() < 2) {
      return;
    }
    const bool foreground = (code == 38);
    const int mode = parse_int_or_default(parts[1], -1);
    if (mode == 5 && parts.size() >= 3) {
      set_indexed_color(foreground, parse_int_or_default(parts[2], 0));
      return;
    }
    if (mode == 2) {
      size_t rgb_index = 2;
      if (parts.size() >= 6) {
        // ISO-8613-6 form with an optional color-space slot:
        // 38:2:<space>:R:G:B (the slot can be empty).
        rgb_index = 3;
      }
      if (parts.size() >= rgb_index + 3) {
        set_rgb_color(foreground, parse_int_or_default(parts[rgb_index], 0),
                      parse_int_or_default(parts[rgb_index + 1], 0),
                      parse_int_or_default(parts[rgb_index + 2], 0));
      }
    }
  };

  const std::vector<std::string_view> params = split_sv(params_str, ';');
  for (size_t i = 0; i < params.size(); ++i) {
    const std::string_view token = params[i];
    if (token.find(':') != std::string_view::npos) {
      apply_colon_extended(token);
      continue;
    }

    int code = parse_int_or_default(token, 0);
    if (code == 38 || code == 48) {
      if (i + 1 >= params.size()) {
        continue;
      }
      const int mode = parse_int_or_default(params[i + 1], -1);
      const bool foreground = (code == 38);
      if (mode == 5 && i + 2 < params.size()) {
        set_indexed_color(foreground, parse_int_or_default(params[i + 2], 0));
        i += 2;
        continue;
      }
      if (mode == 2 && i + 4 < params.size()) {
        set_rgb_color(foreground, parse_int_or_default(params[i + 2], 0),
                      parse_int_or_default(params[i + 3], 0),
                      parse_int_or_default(params[i + 4], 0));
        i += 4;
        continue;
      }
      continue;
    }
    apply_basic(code);
  }
}

void TerminalEmulator::handle_osc() {
  // Swallow OSC sequences (window title, etc.)
}

void TerminalEmulator::handle_csi(char final_char) {
  std::string params_str = csi_buf_;
  bool private_mode = false;
  if (!params_str.empty() && params_str[0] == '?') {
    private_mode = true;
    params_str.erase(params_str.begin());
  }
  if (!params_str.empty() && (params_str[0] == '>' || params_str[0] == '=')) {
    return; // DA2 etc., ignore
  }
  // Check for intermediate bytes (space etc.) - skip unknown sequences
  bool has_intermediate = false;
  for (char c : params_str) {
    if (c >= 0x20 && c <= 0x2F) {
      has_intermediate = true;
      break;
    }
  }
  if (has_intermediate) return;

  const std::vector<int> p = parse_params(params_str);

  switch (final_char) {
  case 'A': // Cursor Up
    wrap_pending_ = false;
    cursor_row_ -= get_param(p, 0, 1);
    break;
  case 'B': // Cursor Down
    wrap_pending_ = false;
    cursor_row_ += get_param(p, 0, 1);
    break;
  case 'C': // Cursor Forward
    wrap_pending_ = false;
    cursor_col_ += get_param(p, 0, 1);
    break;
  case 'D': // Cursor Backward
    wrap_pending_ = false;
    cursor_col_ -= get_param(p, 0, 1);
    break;
  case 'E': // Cursor Next Line
    wrap_pending_ = false;
    cursor_row_ += get_param(p, 0, 1);
    cursor_col_ = 0;
    break;
  case 'F': // Cursor Previous Line
    wrap_pending_ = false;
    cursor_row_ -= get_param(p, 0, 1);
    cursor_col_ = 0;
    break;
  case 'G': // Cursor Character Absolute
    wrap_pending_ = false;
    cursor_col_ = get_param(p, 0, 1) - 1;
    break;
  case 'd': // Line Position Absolute
    wrap_pending_ = false;
    cursor_row_ = get_param(p, 0, 1) - 1;
    break;
  case 'H': // Cursor Position
  case 'f': {
    wrap_pending_ = false;
    cursor_row_ = get_param(p, 0, 1) - 1;
    cursor_col_ = get_param(p, 1, 1) - 1;
    break;
  }
  case 'J': { // Erase in Display
    const int mode = get_param(p, 0, 0);
    if (mode == 2 || mode == 3) {
      clear_all();
      cursor_row_ = 0;
      cursor_col_ = 0;
    } else if (mode == 0) {
      for (int c = cursor_col_; c < width_; ++c)
        cell_mut(cursor_row_, c) = blank_cell();
      for (int r = cursor_row_ + 1; r < height_; ++r)
        clear_line(r);
    } else if (mode == 1) {
      for (int r = 0; r < cursor_row_; ++r)
        clear_line(r);
      for (int c = 0; c <= cursor_col_ && c < width_; ++c)
        cell_mut(cursor_row_, c) = blank_cell();
    }
    break;
  }
  case 'K': { // Erase in Line
    const int mode = get_param(p, 0, 0);
    if (mode == 0) {
      for (int c = cursor_col_; c < width_; ++c)
        cell_mut(cursor_row_, c) = blank_cell();
    } else if (mode == 1) {
      for (int c = 0; c <= cursor_col_ && c < width_; ++c)
        cell_mut(cursor_row_, c) = blank_cell();
    } else if (mode == 2) {
      clear_line(cursor_row_);
    }
    break;
  }
  case 'L': { // Insert Lines
    const int n = get_param(p, 0, 1);
    if (cursor_row_ >= scroll_top_ && cursor_row_ <= scroll_bottom_) {
      scroll_down_region(cursor_row_, scroll_bottom_, n);
    }
    break;
  }
  case 'M': { // Delete Lines
    const int n = get_param(p, 0, 1);
    if (cursor_row_ >= scroll_top_ && cursor_row_ <= scroll_bottom_) {
      scroll_up_region(cursor_row_, scroll_bottom_, n);
    }
    break;
  }
  case '@': { // Insert Characters
    const int n = std::min(get_param(p, 0, 1), width_ - cursor_col_);
    for (int c = width_ - 1; c >= cursor_col_ + n; --c)
      cell_mut(cursor_row_, c) = cell(cursor_row_, c - n);
    for (int c = cursor_col_; c < cursor_col_ + n && c < width_; ++c)
      cell_mut(cursor_row_, c) = blank_cell();
    break;
  }
  case 'P': { // Delete Characters
    const int n = std::min(get_param(p, 0, 1), width_ - cursor_col_);
    for (int c = cursor_col_; c < width_ - n; ++c)
      cell_mut(cursor_row_, c) = cell(cursor_row_, c + n);
    for (int c = width_ - n; c < width_; ++c)
      cell_mut(cursor_row_, c) = blank_cell();
    break;
  }
  case 'X': { // Erase Characters
    const int n = std::min(get_param(p, 0, 1), width_ - cursor_col_);
    for (int c = cursor_col_; c < cursor_col_ + n; ++c)
      cell_mut(cursor_row_, c) = blank_cell();
    break;
  }
  case 'S': { // Scroll Up
    if (!private_mode) {
      scroll_up_region(scroll_top_, scroll_bottom_, get_param(p, 0, 1));
    }
    break;
  }
  case 'T': { // Scroll Down
    if (!private_mode) {
      scroll_down_region(scroll_top_, scroll_bottom_, get_param(p, 0, 1));
    }
    break;
  }
  case 'r': { // Set Scrolling Region (DECSTBM)
    if (!private_mode) {
      int top = get_param(p, 0, 1) - 1;
      int bottom = get_param(p, 1, height_) - 1;
      top = clamp_int(top, 0, height_ - 1);
      bottom = clamp_int(bottom, 0, height_ - 1);
      if (top < bottom) {
        scroll_top_ = top;
        scroll_bottom_ = bottom;
      }
      cursor_row_ = 0;
      cursor_col_ = 0;
      wrap_pending_ = false;
    }
    break;
  }
  case 'm': // SGR
    handle_sgr(params_str);
    break;
  case 's': // Save cursor
    if (!private_mode) {
      saved_row_ = cursor_row_;
      saved_col_ = cursor_col_;
    }
    break;
  case 'u': // Restore cursor
    wrap_pending_ = false;
    cursor_row_ = saved_row_;
    cursor_col_ = saved_col_;
    break;
  case 'l': // Reset Mode
  case 'h': // Set Mode
    (void)private_mode;
    break;
  case 'c': // Device Attributes (DA1)
    if (!private_mode) {
      // Report as VT220 with 256-color support
      response_buf_ += "\x1b[?62;22c";
    }
    break;
  case 'n': // Device Status Report
    if (!private_mode) {
      int mode = get_param(p, 0, 0);
      if (mode == 6) {
        // CPR: report cursor position (1-based)
        response_buf_ += "\x1b[";
        response_buf_ += std::to_string(cursor_row_ + 1);
        response_buf_ += ';';
        response_buf_ += std::to_string(cursor_col_ + 1);
        response_buf_ += 'R';
      } else if (mode == 5) {
        // Status report: "OK"
        response_buf_ += "\x1b[0n";
      }
    }
    break;
  default:
    break;
  }

  clamp_cursor();
}

void TerminalEmulator::feed(std::string_view bytes) {
  for (size_t i = 0; i < bytes.size(); ++i) {
    unsigned char byte = static_cast<unsigned char>(bytes[i]);

    // Skip bytes for charset designator sequences (ESC ( B etc.)
    if (skip_bytes_ > 0) {
      --skip_bytes_;
      continue;
    }

    // UTF-8 continuation byte collection
    if (utf8_remaining_ > 0) {
      if ((byte & 0xC0) == 0x80) {
        utf8_cp_ = (utf8_cp_ << 6) | (byte & 0x3F);
        if (--utf8_remaining_ == 0) {
          put_codepoint(utf8_cp_);
        }
        continue;
      }
      // Bad continuation: discard partial codepoint, re-process byte
      utf8_remaining_ = 0;
    }

    switch (state_) {
    case ParseState::NORMAL:
      if (byte == 0x1b) {
        state_ = ParseState::ESC;
      } else if (byte == '\r') {
        wrap_pending_ = false;
        carriage_return();
      } else if (byte == '\n' || byte == '\x0b' || byte == '\x0c') {
        wrap_pending_ = false;
        newline();
      } else if (byte == '\b') {
        wrap_pending_ = false;
        backspace();
      } else if (byte == '\t') {
        wrap_pending_ = false;
        tab();
      } else if (byte == '\x07') {
        // BEL - ignore
      } else if (byte == '\x0e' || byte == '\x0f') {
        // SO/SI (shift out/in) - ignore
      } else if (byte >= 0x20 && byte < 0x80) {
        put_codepoint(static_cast<char32_t>(byte));
      } else if ((byte & 0xE0) == 0xC0) {
        utf8_cp_ = byte & 0x1F;
        utf8_remaining_ = 1;
      } else if ((byte & 0xF0) == 0xE0) {
        utf8_cp_ = byte & 0x0F;
        utf8_remaining_ = 2;
      } else if ((byte & 0xF8) == 0xF0) {
        utf8_cp_ = byte & 0x07;
        utf8_remaining_ = 3;
      }
      // else: ignore C1 control codes, invalid bytes
      break;

    case ParseState::ESC:
      if (byte == '[') {
        state_ = ParseState::CSI;
        csi_buf_.clear();
      } else if (byte == ']') {
        state_ = ParseState::OSC;
        osc_buf_.clear();
      } else if (byte == 'c') {
        reset();
      } else if (byte == '7') {
        saved_row_ = cursor_row_;
        saved_col_ = cursor_col_;
        state_ = ParseState::NORMAL;
      } else if (byte == '8') {
        cursor_row_ = saved_row_;
        cursor_col_ = saved_col_;
        clamp_cursor();
        state_ = ParseState::NORMAL;
      } else if (byte == 'M') {
        wrap_pending_ = false;
        reverse_index();
        state_ = ParseState::NORMAL;
      } else if (byte == 'D') {
        wrap_pending_ = false;
        newline();
        state_ = ParseState::NORMAL;
      } else if (byte == 'E') {
        wrap_pending_ = false;
        carriage_return();
        newline();
        state_ = ParseState::NORMAL;
      } else if (byte == '(' || byte == ')' || byte == '*' || byte == '+') {
        // Character set designation: skip the next byte (charset name)
        skip_bytes_ = 1;
        state_ = ParseState::NORMAL;
      } else {
        state_ = ParseState::NORMAL;
      }
      break;

    case ParseState::CSI:
      if (byte >= 0x40 && byte <= 0x7e) {
        handle_csi(static_cast<char>(byte));
        state_ = ParseState::NORMAL;
        csi_buf_.clear();
      } else if (byte >= 0x20 && byte < 0x40) {
        if (csi_buf_.size() < 256) {
          csi_buf_.push_back(static_cast<char>(byte));
        }
      } else {
        // Invalid byte in CSI, abort
        state_ = ParseState::NORMAL;
        csi_buf_.clear();
      }
      break;

    case ParseState::OSC:
      if (byte == 0x07) {
        handle_osc();
        state_ = ParseState::NORMAL;
      } else if (byte == 0x1b) {
        state_ = ParseState::OSC_ESC;
      } else {
        if (osc_buf_.size() < 4096) {
          osc_buf_.push_back(static_cast<char>(byte));
        }
      }
      break;

    case ParseState::OSC_ESC:
      if (byte == '\\') {
        handle_osc();
      }
      state_ = ParseState::NORMAL;
      break;
    }
  }
}
