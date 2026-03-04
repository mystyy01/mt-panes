#include "terminal.hpp"
#include <cstdlib>
#include <fcntl.h>
#include <signal.h>


// terminal class method bodies

Terminal::Terminal(int _term_id, pid_t _pid, int _master_fd){
    term_id = _term_id;
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
void Terminal::set_size(int cols, int rows){
    struct winsize ws{};
    ws.ws_col = static_cast<unsigned short>(cols);
    ws.ws_row = static_cast<unsigned short>(rows);
    ioctl(master_fd, TIOCSWINSZ, &ws);
}
void Terminal::close_term(){
    close(master_fd);
}


// terminal manager class method bodies

int TerminalManager::new_terminal(){
    // open a new terminal and append it to the terminal vector
    int term_id = next_term_id++;
    int master_fd;
    pid_t pid = forkpty(&master_fd, nullptr, nullptr, nullptr);
    if (pid < 0) {
        // fork failed
        perror("forkpty");
        return -1;
    }
    if (pid == 0) {
        // child – use the same shell that launched this program
        const char *outer_term = getenv("MTPANES_OUTER_TERM");
        if (outer_term != nullptr && outer_term[0] != '\0') {
            setenv("TERM", outer_term, 1);
        }
        const char *color_term = getenv("COLORTERM");
        if (color_term == nullptr || color_term[0] == '\0') {
            setenv("COLORTERM", "truecolor", 1);
        }

        const char *shell = getenv("SHELL");
        if (!shell || shell[0] == '\0') {
            shell = "/bin/sh";
        }
        execl(shell, shell, (char*)nullptr);
        perror("execl");
        _exit(127);
    }
    int flags = fcntl(master_fd, F_GETFL, 0);
    if (flags >= 0) {
        (void)fcntl(master_fd, F_SETFL, flags | O_NONBLOCK);
    }
    Terminal new_term = Terminal(term_id, pid, master_fd);
    terminals.push_back(new_term);
    // new_term.update_term();
    // dont close here 
    return term_id;
};
bool TerminalManager::close_terminal(int t_id){
    for (auto it = terminals.begin(); it != terminals.end(); ++it) {
        if (it->term_id != t_id) {
            continue;
        }
        kill(it->pid, SIGHUP);
        it->close_term();
        waitpid(it->pid, nullptr, WNOHANG);
        terminals.erase(it);
        return true;
    }
    return false;
}
std::vector<int> TerminalManager::collect_exited_terminals() {
    std::vector<int> exited;
    for (auto it = terminals.begin(); it != terminals.end();) {
        int status = 0;
        pid_t rc = waitpid(it->pid, &status, WNOHANG);
        if (rc == 0) {
            ++it;
            continue;
        }
        if (rc == it->pid || (rc < 0 && errno == ECHILD)) {
            exited.push_back(it->term_id);
            it->close_term();
            it = terminals.erase(it);
            continue;
        }
        ++it;
    }
    return exited;
}
Terminal* TerminalManager::get_term(int t_id){
    // return terminal object from id
    for (auto &term : terminals) {
        if (term.term_id == t_id) {
            return &term;
        }
    }
    return nullptr;
}
std::vector<Terminal>* TerminalManager::get_all_terminals(){
    return &terminals;
}
