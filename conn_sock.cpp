#include "conn.hpp"
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <cerrno>

class ConnSock : public Conn {
    int fd = -1;
    std::string path;
    int goat_id = -1;

    void log(const char* text) const {
        time_t now = time(nullptr);
        char tbuf[20];
        strftime(tbuf, sizeof(tbuf), "%H:%M:%S", localtime(&now));
        printf("[%s] [PID %d] [sock %d] %s\n", tbuf, getpid(), goat_id, text);
        fflush(stdout);
    }

public:
    ConnSock(int id, bool create) : goat_id(id) {
        char buf[128];
        snprintf(buf, sizeof(buf), "/tmp/wolf_goat_%d.sock", id);
        path = buf;

        if (create) {
            // Волк в сервер
            unlink(path.c_str());
            fd = socket(AF_UNIX, SOCK_STREAM, 0);
            if (fd == -1) { perror("socket"); _exit(1); }

            struct sockaddr_un addr{};
            addr.sun_family = AF_UNIX;
            strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

            if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
                perror("bind"); _exit(1);
            }
            if (listen(fd, 1) == -1) { perror("listen"); _exit(1); }
            log("сокет создан и слушает");
        } else {
            // Козлёнок в клиент
            fd = socket(AF_UNIX, SOCK_STREAM, 0);
            if (fd == -1) { perror("socket client"); _exit(1); }

            struct sockaddr_un addr{};
            addr.sun_family = AF_UNIX;
            strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

            while (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
                if (errno == ENOENT || errno == ECONNREFUSED) {
                    usleep(100000);
                    continue;
                }
                perror("connect");
                _exit(1);
            }
            log("подключился к волку");
        }
    }

    // Волк принимает подключение сразу после fork()
    void wolf_accept() {
        if (fd == -1) return;
        int client_fd = accept(fd, nullptr, nullptr);
        if (client_fd == -1) { perror("accept"); _exit(1); }
        close(fd);
        fd = client_fd;
        log("принял подключение козлёнка");
    }

    bool read(Message& msg) override {
        struct pollfd pfd = { fd, POLLIN, 0 };
        int r = poll(&pfd, 1, 5000);
        if (r == 0) {
            log("таймаут чтения");
            return false;
        }
        if (r < 0) { perror("poll"); return false; }

        ssize_t n = ::read(fd, &msg, sizeof(msg));
        if (n != sizeof(msg)) {
            if (n >= 0) log("неполное чтение");
            else perror("read");
            return false;
        }
        return true;
    }

    bool write(const Message& msg) override {
        ssize_t n = ::write(fd, &msg, sizeof(msg));
        if (n != sizeof(msg)) {
            perror("write");
            return false;
        }
        return true;
    }

    std::string name() const override { return "sock"; }

    ~ConnSock() override {
        if (fd != -1) {
            shutdown(fd, SHUT_RDWR);
            close(fd);
        }
        if (getpid() == getppid()) {
            unlink(path.c_str());
            log("сокет удалён");
        }
    }
};