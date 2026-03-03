#pragma once


#include <pty.h>        // or <util.h> on BSD/macOS
#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string>
#include <iostream>
#include <vector>
#include <poll.h>
#include <sys/wait.h>
#include <cerrno>

class Terminal{
public:
    int term_id;
    pid_t pid;
    int master_fd;
    Terminal(int _term_id, pid_t _pid, int _master_fd);
    void update_term();
    std::string read_available();
    void send_cmd(std::string cmd);
    void close_term();
};

class TerminalManager{
public:
    int new_terminal();
    bool close_terminal(int t_id);
    Terminal *get_term(int t_id);
    std::vector<Terminal> *get_all_terminals();
private:
    int next_term_id = 1;
    std::vector<Terminal> terminals;
};  
