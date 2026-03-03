#include "terminal.hpp"
#include <fcntl.h>


// terminal class method bodies

Terminal::Terminal(pid_t _pid, int _master_fd){
    pid = _pid;
    master_fd = _master_fd;
};
void Terminal::update_term(){
    char buf[1024];
    ssize_t n;
    while ((n = read(master_fd, buf, sizeof(buf))) > 0) {
        std::cout.write(buf, n);
        std::cout.flush();
    }
}
std::string Terminal::read_available() {
    std::string out;
    char buf[4096];
    while (true) {
        ssize_t n = read(master_fd, buf, sizeof(buf));
        if (n > 0) {
            out.append(buf, static_cast<size_t>(n));
            continue;
        }
        if (n == 0) {
            break;
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
        }
        break;
    }
    return out;
}
void Terminal::send_cmd(std::string cmd){
    write(master_fd, cmd.data(), cmd.size());
};
void Terminal::close_term(){
    close(master_fd);
}


// terminal manager class method bodies

int TerminalManager::new_terminal(){
    // open a new terminal and append it to the terminal vector
    int term_id = terminals.size() + 1;
    int master_fd;
    pid_t pid = forkpty(&master_fd, nullptr, nullptr, nullptr);
    if (pid < 0) {
        // fork failed
        perror("forkpty");
        return -1;
    }
    if (pid == 0) {
        // child
        execl("/bin/bash", "bash", (char*)nullptr);
        perror("execl");
        _exit(127);
    }
    int flags = fcntl(master_fd, F_GETFL, 0);
    if (flags >= 0) {
        (void)fcntl(master_fd, F_SETFL, flags | O_NONBLOCK);
    }
    Terminal new_term = Terminal(pid, master_fd);
    terminals.push_back(new_term);
    // new_term.update_term();
    // dont close here 
    return term_id;
};
Terminal* TerminalManager::get_term(int t_id){
    // return terminal object from id
    return &terminals.at(t_id-1);
}
std::vector<Terminal>* TerminalManager::get_all_terminals(){
    return &terminals;
}
