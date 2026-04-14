#include "conn.hpp"
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <ctime>

class ConnMq : public Conn {
    mqd_t mq = -1;
    std::string mq_name;
    int goat_id;

    void log(const char* text) const {
        time_t now = time(nullptr);
        char tbuf[20];
        strftime(tbuf, sizeof(tbuf), "%H:%M:%S", localtime(&now));
        printf("[%s] [PID %d] [mq %d] %s\n", tbuf, getpid(), goat_id, text);
        fflush(stdout);
    }

public:
    ConnMq(int id, bool create) : goat_id(id) {
        char buf[64];
        snprintf(buf, sizeof(buf), "/wolf_goat_%d", id);
        mq_name = buf;

        if (create) {
            struct mq_attr attr{};
            attr.mq_maxmsg = 10;
            attr.mq_msgsize = sizeof(Message);
            mq = mq_open(mq_name.c_str(), O_CREAT | O_RDWR, 0666, &attr);
            if (mq == -1) { perror("mq_open create"); _exit(1); }
            log("очередь создана");
        } else {
            mq = mq_open(mq_name.c_str(), O_RDWR);
            if (mq == -1) { perror("mq_open"); _exit(1); }
            log("подключился");
        }
    }

    bool read(Message& msg) override {
        struct timespec ts{};
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 5;

        ssize_t n = mq_timedreceive(mq, (char*)&msg, sizeof(msg), nullptr, &ts);
        if (n == -1) {
            if (errno == ETIMEDOUT) {
                log("таймаут чтения - волк ушёл, выход");
                return false;
            }
            perror("mq_timedreceive");
            return false;
        }
        return true;
    }

    bool write(const Message& msg) override {
        struct timespec ts{};
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 5;

        if (mq_timedsend(mq, (const char*)&msg, sizeof(msg), 0, &ts) == -1) {
            if (errno != ETIMEDOUT) perror("mq_timedsend");
            return false;
        }
        return true;
    }

    std::string name() const override { return "mq"; }

    ~ConnMq() override {
        if (mq != -1) mq_close(mq);
        if (getpid() == getppid()) {  // только волк
            mq_unlink(mq_name.c_str());
            log("очередь удалена");
        }
    }
};