#define _XOPEN_SOURCE 600

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

class Pane{
public:
    int term_id;
    int pane_id;
    Pane(int term_id){
        if (term_id < 0){
            return;
        }
    }
};

class Terminal{
public:
    pid_t pid;
    int master_fd;
    Terminal(pid_t _pid, int _master_fd){
        pid = _pid;
        master_fd = _master_fd;
    };
    void update_term(){
        char buf[1024];
        ssize_t n;
        while ((n = read(master_fd, buf, sizeof(buf))) > 0) {
            std::cout.write(buf, n);
            std::cout.flush();
        }
    }
    void send_cmd(std::string cmd){
        write(master_fd, cmd.data(), cmd.size());
    };
    void close_term(){
        close(master_fd);
    }
};

class TerminalManager{
public:
    int new_terminal(){
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
            execl("/bin/zsh", "zsh", (char*)nullptr);
            perror("execl");
            _exit(127);
        }
        Terminal new_term = Terminal(pid, master_fd);
        terminals.push_back(new_term);
        // new_term.update_term();
        // dont close here 
        return term_id;
    };
    Terminal get_term(int t_id){
        // return terminal object from id
        return terminals.at(t_id-1);
    }
private:
    std::vector<Terminal> terminals;

};  

int main() {
    // int master_fd;
    // pid_t pid = forkpty(&master_fd, nullptr, nullptr, nullptr);
    // if (pid < 0) {
    //     perror("forkpty");
    //     return 1;
    // }

    // if (pid == 0) {
    //     // Child
    //     execl("/bin/bash", "bash", (char*)nullptr);
    //     perror("execl");
    //     _exit(127);
    // }

    // // Parent
    // std::string cmd = "echo hello from C++\n";
    // write(master_fd, cmd.data(), cmd.size());

    // char buf[1024];
    // ssize_t n;
    // while ((n = read(master_fd, buf, sizeof(buf))) > 0) {
    //     std::cout.write(buf, n);
    //     std::cout.flush();
    // }

    // close(master_fd);
    // return 0;

    TerminalManager t_manager;
    int term_id = t_manager.new_terminal();
    if (term_id < 0){
        exit(-1);
    }
    Terminal term = t_manager.get_term(term_id);
    int fd = term.master_fd;
    pid_t child_pid = term.pid;

    termios orig{};
    bool raw_mode_enabled = false;
    if (isatty(STDIN_FILENO)) {
        if (tcgetattr(STDIN_FILENO, &orig) == 0) {
            termios raw = orig;
            cfmakeraw(&raw);
            if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0) {
                raw_mode_enabled = true;
            } else {
                perror("tcsetattr");
            }
        } else {
            perror("tcgetattr");
        }
    }

    bool running = true;
    pollfd watch[2];
    watch[0] = {STDIN_FILENO, POLLIN, 0};
    watch[1] = {fd, POLLIN | POLLHUP | POLLERR, 0};
    while (running) {
        int rc = poll(watch, 2, -1);
        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("poll");
            break;
        }

        if (watch[0].revents & POLLIN) {
            char inbuf[1024];
            ssize_t n = read(STDIN_FILENO, inbuf, sizeof(inbuf));
            if (n == 0) {
                running = false;
            } else if (n > 0) {
                ssize_t total_written = 0;
                while (total_written < n) {
                    ssize_t written = write(fd, inbuf + total_written, static_cast<size_t>(n - total_written));
                    if (written <= 0) {
                        if (written < 0 && errno == EINTR) {
                            continue;
                        }
                        perror("write");
                        running = false;
                        break;
                    }
                    total_written += written;
                }
            } else if (errno != EINTR) {
                perror("read stdin");
                running = false;
            }
        }

        if (watch[1].revents & POLLIN) {
            char buf[1024];
            ssize_t n = read(fd, buf, sizeof(buf));
            if (n > 0) {
                ssize_t total_written = 0;
                while (total_written < n) {
                    ssize_t written = write(STDOUT_FILENO, buf + total_written, static_cast<size_t>(n - total_written));
                    if (written <= 0) {
                        if (written < 0 && errno == EINTR) {
                            continue;
                        }
                        perror("write stdout");
                        running = false;
                        break;
                    }
                    total_written += written;
                }
            } else if (n == 0) {
                running = false;
            } else if (errno != EINTR) {
                perror("read");
                running = false;
            }
        }

        if (watch[0].revents & (POLLHUP | POLLERR | POLLNVAL)) {
            running = false;
        }
        if (watch[1].revents & (POLLHUP | POLLERR | POLLNVAL)) {
            running = false;
        }
    }

    if (raw_mode_enabled) {
        tcsetattr(STDIN_FILENO, TCSANOW, &orig);
    }
    close(fd);
    waitpid(child_pid, nullptr, 0);
    return 0;
}
