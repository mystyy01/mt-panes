#pragma once

#include <string>
#include <string_view>
#include <vector>

struct TerminalCell {
  char ch;
  int fg;
  int bg;
  bool bold;
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

private:
  enum class ParseState { NORMAL, ESC, CSI };

  int width_;
  int height_;
  int cursor_row_;
  int cursor_col_;
  int saved_row_;
  int saved_col_;

  int fg_;
  int bg_;
  bool bold_;

  ParseState state_;
  std::string csi_buf_;
  std::vector<TerminalCell> cells_;

  TerminalCell blank_cell() const;
  TerminalCell &cell_mut(int row, int col);
  void clamp_cursor();
  void clear_all();
  void clear_line(int row);
  void scroll_up(int lines);
  void newline();
  void carriage_return();
  void backspace();
  void tab();
  void put_char(char c);
  void handle_csi(char final_char);
};
