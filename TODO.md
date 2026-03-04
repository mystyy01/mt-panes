# TODO

- [ ] Make `exit` in the shell close the pane
- [ ] Fix mode-switch lag (INSERT→NORMAL has ~1s delay, likely ncurses ESCDELAY; maybe increase fps cap too)
- [ ] Hyprland-style balanced tiling layout (find optimal split direction to balance the screen)
- [ ] Vim-like normal mode motions (`w`, `b` to hop between words, etc.)
- [ ] Keybind to open a new pane inheriting the focused pane's CWD and env variables
- [ ] Alt+mouse drag to move/rearrange panes
- [ ] Fix NVIM in mt-panes, file explorer doesnt work with enter because it gets intercepted by the backend - rather than sending to nvim
