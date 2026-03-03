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
    pid_t pid;
    int master_fd;
    Terminal(pid_t _pid, int _master_fd);
    void update_term();
    std::string read_available();
    void send_cmd(std::string cmd);
    void close_term();
};

class TerminalManager{
public:
    int new_terminal();
    Terminal *get_term(int t_id);
    std::vector<Terminal> *get_all_terminals();
private:
    std::vector<Terminal> terminals;
};  
