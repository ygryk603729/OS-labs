#include "conn.hpp"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <cerrno>

class ConnFifo : public Conn {
    std::string path_to_wolf;     
    std::string path_from_wolf;   
    int fd_write = -1;            // для записи в сторону волка / для чтения у волка
    int fd_read  = -1;            // для чтения от волка / для записи у волка
    int goat_id  = -1;

    void log(const char* text) const {
        time_t now = time(nullptr);
        char tbuf[20];
        strftime(tbuf, sizeof(tbuf), "%H:%M:%S", localtime(&now));
        printf("[%s] [PID %d] [fifo %d] %s\n", tbuf, getpid(), goat_id, text);
        fflush(stdout);
    }

public:
    ConnFifo(int id, bool create) : goat_id(id) {
        char buf1[128], buf2[128];
        snprintf(buf1, sizeof(buf1), "/tmp/wolf_goat_%d_to",   id);
        snprintf(buf2, sizeof(buf2), "/tmp/wolf_goat_%d_from", id);
        path_to_wolf   = buf1;
        path_from_wolf = buf2;

        if (create) {
            // Волк создаёт FIFO, не открывает их здесь.
            // Открытие будет после всех fork()
            unlink(path_to_wolf.c_str());
            unlink(path_from_wolf.c_str());
            if (mkfifo(path_to_wolf.c_str(),   0666) == -1 && errno != EEXIST) perror("mkfifo to");
            if (mkfifo(path_from_wolf.c_str(), 0666) == -1 && errno != EEXIST) perror("mkfifo from");
            log("FIFO созданы (открытие отложено)");
        } else {
            // Козлёнок открывает сразу после fork
            fd_write = open(path_to_wolf.c_str(), O_WRONLY);
            if (fd_write == -1) { perror("goat open to_wolf"); _exit(1); }
            log("открыл запись → волк");

            fd_read = open(path_from_wolf.c_str(), O_RDONLY);
            if (fd_read == -1) { perror("goat open from_wolf"); _exit(1); }
            log("открыл чтение ← волк");
        }
    }

    // Волк вызывает это после того, как все козлята уже созданы и открылись
    void wolf_open_pipes() {
        fd_read  = open(path_to_wolf.c_str(), O_RDONLY);
        if (fd_read == -1) { perror("wolf open to_wolf RDONLY"); _exit(1); }
        log("открыл чтение ← козлёнок");

        fd_write = open(path_from_wolf.c_str(), O_WRONLY);
        if (fd_write == -1) { perror("wolf open from_wolf WRONLY"); _exit(1); }
        log("открыл запись → козлёнок");
    }

    bool read(Message& msg) override {
        struct pollfd pfd = { fd_read, POLLIN, 0 };
        int r = poll(&pfd, 1, 5000);
        if (r <= 0) {
            if (r == 0) log("таймаут чтения");
            else perror("poll fifo");
            return false;
        }

        ssize_t n = ::read(fd_read, &msg, sizeof(msg));
        if (n != sizeof(msg)) {
            if (n >= 0) log("неполное чтение");
            else if (errno != EAGAIN && errno != EINTR) perror("read fifo");
            return false;
        }
        return true;
    }

    bool write(const Message& msg) override {
        ssize_t n = ::write(fd_write, &msg, sizeof(msg));
        if (n != sizeof(msg)) {
            perror("write fifo");
            return false;
        }
        return true;
    }

    std::string name() const override { return "fifo"; }

    ~ConnFifo() override {
        if (fd_read  != -1) close(fd_read);
        if (fd_write != -1) close(fd_write);
        if (getpid() == getppid()) { 
            unlink(path_to_wolf.c_str());
            unlink(path_from_wolf.c_str());
            log("FIFO удалены");
        }
    }
};