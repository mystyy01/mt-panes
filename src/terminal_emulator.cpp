#include "terminal_emulator.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>

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
} // namespace

TerminalEmulator::TerminalEmulator(int width, int height)
    : width_(1), height_(1), cursor_row_(0), cursor_col_(0), saved_row_(0),
      saved_col_(0), fg_(-1), bg_(-1), bold_(false), state_(ParseState::NORMAL) {
  resize(width, height);
}

TerminalCell TerminalEmulator::blank_cell() const {
  return TerminalCell{.ch = ' ', .fg = -1, .bg = -1, .bold = false};
}

void TerminalEmulator::reset() {
  cursor_row_ = 0;
  cursor_col_ = 0;
  saved_row_ = 0;
  saved_col_ = 0;
  fg_ = -1;
  bg_ = -1;
  bold_ = false;
  state_ = ParseState::NORMAL;
  csi_buf_.clear();
  clear_all();
}

void TerminalEmulator::resize(int width, int height) {
  width = std::max(1, width);
  height = std::max(1, height);
  if (width == width_ && height == height_ && !cells_.empty()) {
    return;
  }

  std::vector<TerminalCell> next(static_cast<size_t>(width * height),
                                 blank_cell());
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
  clamp_cursor();
}

int TerminalEmulator::width() const { return width_; }
int TerminalEmulator::height() const { return height_; }
int TerminalEmulator::cursor_row() const { return cursor_row_; }
int TerminalEmulator::cursor_col() const { return cursor_col_; }

const TerminalCell &TerminalEmulator::cell(int row, int col) const {
  static const TerminalCell kEmpty{' ', -1, -1, false};
  if (row < 0 || col < 0 || row >= height_ || col >= width_ || cells_.empty()) {
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
  std::fill(cells_.begin(), cells_.end(), blank_cell());
}

void TerminalEmulator::clear_line(int row) {
  if (row < 0 || row >= height_) {
    return;
  }
  for (int c = 0; c < width_; ++c) {
    cell_mut(row, c) = blank_cell();
  }
}

void TerminalEmulator::scroll_up(int lines) {
  lines = std::max(0, lines);
  if (lines <= 0 || lines > height_) {
    return;
  }
  for (int r = 0; r < height_ - lines; ++r) {
    for (int c = 0; c < width_; ++c) {
      cell_mut(r, c) = cell(r + lines, c);
    }
  }
  for (int r = height_ - lines; r < height_; ++r) {
    clear_line(r);
  }
}

void TerminalEmulator::newline() {
  if (cursor_row_ >= height_ - 1) {
    scroll_up(1);
    cursor_row_ = height_ - 1;
  } else {
    ++cursor_row_;
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

void TerminalEmulator::put_char(char c) {
  if (cursor_col_ >= width_) {
    carriage_return();
    newline();
  }
  cell_mut(cursor_row_, cursor_col_) =
      TerminalCell{.ch = c, .fg = fg_, .bg = bg_, .bold = bold_};
  ++cursor_col_;
  if (cursor_col_ >= width_) {
    carriage_return();
    newline();
  }
}

void TerminalEmulator::handle_csi(char final_char) {
  bool private_mode = false;
  std::string params = csi_buf_;
  if (!params.empty() && params[0] == '?') {
    private_mode = true;
    params.erase(params.begin());
  }
  const std::vector<int> p = parse_params(params);

  switch (final_char) {
  case 'A':
    cursor_row_ -= get_param(p, 0, 1);
    break;
  case 'B':
    cursor_row_ += get_param(p, 0, 1);
    break;
  case 'C':
    cursor_col_ += get_param(p, 0, 1);
    break;
  case 'D':
    cursor_col_ -= get_param(p, 0, 1);
    break;
  case 'G':
    cursor_col_ = get_param(p, 0, 1) - 1;
    break;
  case 'd':
    cursor_row_ = get_param(p, 0, 1) - 1;
    break;
  case 'H':
  case 'f': {
    const int row = get_param(p, 0, 1) - 1;
    const int col = get_param(p, 1, 1) - 1;
    cursor_row_ = row;
    cursor_col_ = col;
    break;
  }
  case 'J': {
    const int mode = get_param(p, 0, 0);
    if (mode == 2 || mode == 3) {
      clear_all();
      cursor_row_ = 0;
      cursor_col_ = 0;
    } else if (mode == 0) {
      for (int r = cursor_row_; r < height_; ++r) {
        const int start = (r == cursor_row_) ? cursor_col_ : 0;
        for (int c = start; c < width_; ++c) {
          cell_mut(r, c) = blank_cell();
        }
      }
    } else if (mode == 1) {
      for (int r = 0; r <= cursor_row_; ++r) {
        const int end = (r == cursor_row_) ? cursor_col_ : (width_ - 1);
        for (int c = 0; c <= end; ++c) {
          cell_mut(r, c) = blank_cell();
        }
      }
    }
    break;
  }
  case 'K': {
    const int mode = get_param(p, 0, 0);
    if (mode == 2) {
      clear_line(cursor_row_);
    } else if (mode == 0) {
      for (int c = cursor_col_; c < width_; ++c) {
        cell_mut(cursor_row_, c) = blank_cell();
      }
    } else if (mode == 1) {
      for (int c = 0; c <= cursor_col_; ++c) {
        cell_mut(cursor_row_, c) = blank_cell();
      }
    }
    break;
  }
  case 'm': {
    if (p.empty()) {
      fg_ = -1;
      bg_ = -1;
      bold_ = false;
      break;
    }
    for (size_t i = 0; i < p.size(); ++i) {
      const int code = (p[i] < 0) ? 0 : p[i];
      if (code == 0) {
        fg_ = -1;
        bg_ = -1;
        bold_ = false;
      } else if (code == 1) {
        bold_ = true;
      } else if (code == 22) {
        bold_ = false;
      } else if (code >= 30 && code <= 37) {
        fg_ = code - 30;
      } else if (code == 39) {
        fg_ = -1;
      } else if (code >= 40 && code <= 47) {
        bg_ = code - 40;
      } else if (code == 49) {
        bg_ = -1;
      } else if (code >= 90 && code <= 97) {
        fg_ = code - 90;
        bold_ = true;
      } else if (code >= 100 && code <= 107) {
        bg_ = code - 100;
      } else if ((code == 38 || code == 48) && i + 2 < p.size() &&
                 p[i + 1] == 5) {
        int n = p[i + 2];
        if (n >= 0 && n <= 7) {
          if (code == 38) {
            fg_ = n;
          } else {
            bg_ = n;
          }
        } else if (n >= 8 && n <= 15) {
          if (code == 38) {
            fg_ = n - 8;
            bold_ = true;
          } else {
            bg_ = n - 8;
          }
        }
        i += 2;
      }
    }
    break;
  }
  case 's':
    saved_row_ = cursor_row_;
    saved_col_ = cursor_col_;
    break;
  case 'u':
    cursor_row_ = saved_row_;
    cursor_col_ = saved_col_;
    break;
  case 'l':
  case 'h':
    (void)private_mode;
    break;
  default:
    break;
  }

  clamp_cursor();
}

void TerminalEmulator::feed(std::string_view bytes) {
  for (unsigned char byte : bytes) {
    if (state_ == ParseState::NORMAL) {
      if (byte == 0x1b) {
        state_ = ParseState::ESC;
      } else if (byte == '\r') {
        carriage_return();
      } else if (byte == '\n') {
        newline();
      } else if (byte == '\b' || byte == 0x7f) {
        backspace();
      } else if (byte == '\t') {
        tab();
      } else if (byte >= 0x20) {
        put_char(static_cast<char>(byte));
      }
      continue;
    }

    if (state_ == ParseState::ESC) {
      if (byte == '[') {
        state_ = ParseState::CSI;
        csi_buf_.clear();
      } else if (byte == 'c') {
        reset();
        state_ = ParseState::NORMAL;
      } else if (byte == '7') {
        saved_row_ = cursor_row_;
        saved_col_ = cursor_col_;
        state_ = ParseState::NORMAL;
      } else if (byte == '8') {
        cursor_row_ = saved_row_;
        cursor_col_ = saved_col_;
        clamp_cursor();
        state_ = ParseState::NORMAL;
      } else {
        state_ = ParseState::NORMAL;
      }
      continue;
    }

    if (byte >= 0x40 && byte <= 0x7e) {
      handle_csi(static_cast<char>(byte));
      state_ = ParseState::NORMAL;
      csi_buf_.clear();
    } else {
      if (csi_buf_.size() < 256) {
        csi_buf_.push_back(static_cast<char>(byte));
      }
    }
  }
}
