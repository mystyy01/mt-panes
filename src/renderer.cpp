#include "renderer.hpp"
#include <curses.h>
#include <memory>
#include <vector>

Renderer::Renderer() {
  initscr();
  cbreak();
  noecho();
  getmaxyx(stdscr, screeny, screenx);
}

void Renderer::draw_terminal(Vector2 pos, Vector2 size, border_style style,
                             int term_id) {}
void Renderer::render(std::vector<std::unique_ptr<Node>>) {
  // mvprintw(screeny / 2, screenx / 2, "hello world");
  // mvprintw(0, 0, "y: %d, x: %d", screeny, screenx);

  refresh();
}

// draw terminal
// takes pos, border style, size, terminal object/id
// redraw every frame/every keypress/update to the read(master_fd) or write()

// seperate tiling manager - give it current pos and size of currently opened
// terminals and return a pos, size of where a new terminal would be use return
// values in draw_terminal();
