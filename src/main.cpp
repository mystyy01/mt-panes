#include "types.hpp"
#include <curses.h>
#include <string>
#include <unordered_map>

#include "renderer.hpp"
#include "terminal.hpp"
#include "tiling_manager.hpp"

// TODO: POLL TERMINAL RESIZE AND RE COMPUTE LAYOUT

int main() {
  Renderer r;
  TilingManager tile_m;
  TerminalManager term_m;

  int term_id = term_m.new_terminal();
  int term_id2 = term_m.new_terminal();
  if (term_id < 0 || term_id2 < 0) {
    return -1;
  }
  tile_m.new_pane(term_id);
  tile_m.new_pane(term_id2);

  int h, w;
  getmaxyx(stdscr, h, w);
  Rect screen_rect = {.x = 0, .y = 0, .w = w, .h = h};
  std::vector<PaneLayout> layouts = tile_m.compute_layout(screen_rect);
  std::unordered_map<int, std::string> terminal_buffers;

  nodelay(stdscr, TRUE);
  keypad(stdscr, TRUE);

  bool running = true;
  while (running) {
    int ch = getch();
    if (ch == 'q' || ch == 'Q') {
      running = false;
      continue;
    }

    int next_h, next_w;
    getmaxyx(stdscr, next_h, next_w);
    if (next_h != h || next_w != w) {
      h = next_h;
      w = next_w;
      screen_rect = Rect{.x = 0, .y = 0, .w = w, .h = h};
      layouts = tile_m.compute_layout(screen_rect);
    }

    std::vector<Terminal> *terms = term_m.get_all_terminals();
    for (size_t i = 0; i < terms->size(); ++i) {
      std::string out = (*terms)[i].read_available();
      if (out.empty()) {
        continue;
      }
      const int current_term_id = static_cast<int>(i) + 1;
      std::string &buffer = terminal_buffers[current_term_id];
      buffer.append(out);
      constexpr size_t kMaxBufferBytes = 100000;
      if (buffer.size() > kMaxBufferBytes) {
        buffer.erase(0, buffer.size() - kMaxBufferBytes);
      }
    }

    r.render(layouts, terminal_buffers);
    napms(16);
  }

  // TerminalManager t_manager;
  // int term_id = t_manager.new_terminal();
  // if (term_id < 0){
  //     exit(-1);
  // }

  // // render(t_manager.get_all_terminals());
  // Terminal term = *t_manager.get_term(term_id);
  // int fd = term.master_fd;
  // pid_t child_pid = term.pid;

  // termios orig{};
  // bool raw_mode_enabled = false;
  // if (isatty(STDIN_FILENO)) {
  //     if (tcgetattr(STDIN_FILENO, &orig) == 0) {
  //         termios raw = orig;
  //         cfmakeraw(&raw);
  //         if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0) {
  //             raw_mode_enabled = true;
  //         } else {
  //             perror("tcsetattr");
  //         }
  //     } else {
  //         perror("tcgetattr");
  //     }
  // }

  // bool running = true;
  // pollfd watch[2];
  // watch[0] = {STDIN_FILENO, POLLIN, 0};
  // watch[1] = {fd, POLLIN | POLLHUP | POLLERR, 0};
  // while (running) {
  //     int rc = poll(watch, 2, -1);
  //     if (rc < 0) {
  //         if (errno == EINTR) {
  //             continue;
  //         }
  //         perror("poll");
  //         break;
  //     }

  //     if (watch[0].revents & POLLIN) {
  //         char inbuf[1024];
  //         ssize_t n = read(STDIN_FILENO, inbuf, sizeof(inbuf));
  //         if (n == 0) {
  //             running = false;
  //         } else if (n > 0) {
  //             ssize_t total_written = 0;
  //             while (total_written < n) {
  //                 ssize_t written = write(fd, inbuf + total_written,
  //                 static_cast<size_t>(n - total_written)); if (written <= 0)
  //                 {
  //                     if (written < 0 && errno == EINTR) {
  //                         continue;
  //                     }
  //                     perror("write");
  //                     running = false;
  //                     break;
  //                 }
  //                 total_written += written;
  //             }
  //         } else if (errno != EINTR) {
  //             perror("read stdin");
  //             running = false;
  //         }
  //     }

  //     if (watch[1].revents & POLLIN) {
  //         char buf[1024];
  //         ssize_t n = read(fd, buf, sizeof(buf));
  //         if (n > 0) {
  //             ssize_t total_written = 0;
  //             while (total_written < n) {
  //                 ssize_t written = write(STDOUT_FILENO, buf + total_written,
  //                 static_cast<size_t>(n - total_written)); if (written <= 0)
  //                 {
  //                     if (written < 0 && errno == EINTR) {
  //                         continue;
  //                     }
  //                     perror("write stdout");
  //                     running = false;
  //                     break;
  //                 }
  //                 total_written += written;
  //             }
  //         } else if (n == 0) {
  //             running = false;
  //         } else if (errno != EINTR) {
  //             perror("read");
  //             running = false;
  //         }
  //     }

  //     if (watch[0].revents & (POLLHUP | POLLERR | POLLNVAL)) {
  //         running = false;
  //     }
  //     if (watch[1].revents & (POLLHUP | POLLERR | POLLNVAL)) {
  //         running = false;
  //     }
  // }

  // if (raw_mode_enabled) {
  //     tcsetattr(STDIN_FILENO, TCSANOW, &orig);
  // }
  // close(fd);
  // waitpid(child_pid, nullptr, 0);
  // return 0;
}
