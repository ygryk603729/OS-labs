#include "conn.hpp"
#include <vector>
#include <memory>
#include <iostream>
#include <thread>
#include <unistd.h>
#include <sys/wait.h>
#include <cstdlib>
#include <ctime>
#include <random>
#include <csignal>
#include <barrier>
#include <future>
#include <chrono>

#ifndef USE_MQ
#define USE_MQ   0
#endif
#ifndef USE_FIFO
#define USE_FIFO 0
#endif
#ifndef USE_SOCK
#define USE_SOCK 0
#endif

#if USE_MQ
    #include "conn_mq.cpp"
    using ConnType = ConnMq;
    constexpr const char* TYPE_NAME = "mq";
#elif USE_FIFO
    #include "conn_fifo.cpp"
    using ConnType = ConnFifo;
    constexpr const char* TYPE_NAME = "fifo";
#elif USE_SOCK
    #include "conn_sock.cpp"
    using ConnType = ConnSock;
    constexpr const char* TYPE_NAME = "sock";
#else
    #error "Выберите тип: USE_MQ=1 или USE_FIFO=1 или USE_SOCK=1"
#endif

std::vector<std::unique_ptr<Conn>> connections;
std::vector<int> goat_status(7, 1);  // 1 значит жив

void goat_process(int id) {
    auto conn = std::make_unique<ConnType>(id, false);

    std::mt19937 rng(static_cast<unsigned>(time(nullptr)) ^ id);
    std::uniform_int_distribution<int> alive_dist(1, 100);
    std::uniform_int_distribution<int> dead_dist(1, 50);

    Message msg{};
    msg.goat_id = id;
    msg.status = GoatStatus::ALIVE;

    while (true) {
        msg.goat_number = (msg.status == GoatStatus::ALIVE) ? alive_dist(rng) : dead_dist(rng);

        if (!conn->write(msg)) break;
        if (!conn->read(msg)) break;

        goat_status[id] = (msg.status == GoatStatus::ALIVE);
        msg.status = static_cast<GoatStatus>(goat_status[id]);
    }
    _exit(0);
}

int main() {
    std::cout << "=== ВОЛК И 7 КОЗЛЯТ [" << TYPE_NAME << "] ===\n\n";

    for (int i = 0; i < 7; ++i) {
        auto conn = std::make_unique<ConnType>(i, true);
        if (fork() == 0) {
            goat_process(i);
        }
        connections.push_back(std::move(conn));
    }

#if USE_FIFO
    for (auto& conn : connections) {
        static_cast<ConnFifo*>(conn.get())->wolf_open_pipes();
    }
#endif
#if USE_SOCK
    for (auto& conn : connections) {
        static_cast<ConnSock*>(conn.get())->wolf_accept();
    }
#endif

    std::mt19937 rng(time(nullptr));
    std::uniform_int_distribution<int> wolf_dist(1, 100);

    int consecutive_dead = 0;
    int round = 0;

    while (consecutive_dead < 2) {
        ++round;

        int wolf_num = -1;
        std::cout << "\n--- Раунд " << round << " | Введите число волка (1-100) или ждите автобросок: ";

        fd_set readfds;
        struct timeval tv{};
        tv.tv_sec = 3;
        tv.tv_usec = 0;

        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);

        int retval = select(STDIN_FILENO + 1, &readfds, nullptr, nullptr, &tv);

        if (retval > 0 && FD_ISSET(STDIN_FILENO, &readfds)) {
            if (scanf("%d", &wolf_num) == 1 && wolf_num >= 1 && wolf_num <= 100) {
                std::cout << wolf_num << " (вручную)\n";
            } else {
                std::cout << "\nНекорректный ввод - автобросок!\n";
                wolf_num = wolf_dist(rng);
                std::cout << "Волк бросил: " << wolf_num << "\n";
            }
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
        } else {
            wolf_num = wolf_dist(rng);
            std::cout << "\nВремя вышло — автобросок: " << wolf_num << "\n";
        }

        std::barrier sync(7);
        std::vector<std::thread> workers;
        std::vector<int> goat_numbers(7, 0); 

        for (int i = 0; i < 7; ++i) {
            workers.emplace_back([i, wolf_num, &sync, &goat_numbers]() {
                Message msg{};
                if (!connections[i]->read(msg)) {
                    goat_status[i] = 0;
                    sync.arrive_and_wait();
                    return;
                }

                goat_numbers[i] = msg.goat_number; 

                const int n = 7;
                const int alive_threshold  = 70 / n;  
                const int revive_threshold = 20 / n;  

                int diff = std::abs(msg.goat_number - wolf_num);
                bool was_alive = (msg.status == GoatStatus::ALIVE);

                msg.status = was_alive
                    ? (diff <= alive_threshold  ? GoatStatus::ALIVE : GoatStatus::DEAD)
                    : (diff <= revive_threshold ? GoatStatus::ALIVE : GoatStatus::DEAD);

                goat_status[i] = (msg.status == GoatStatus::ALIVE);
                connections[i]->write(msg);
                sync.arrive_and_wait();
            });
        }
        for (auto& t : workers) t.join();

        std::cout << "Козлята бросили: ";
        for (int i = 0; i < 7; ++i) {
            std::cout << goat_numbers[i];
            if (i < 6) std::cout << ", ";
        }
        std::cout << "\n";

        int alive_count = 0;
        for (int s : goat_status) alive_count += s;

        std::cout << "Живых: " << alive_count << " | Мёртвых: " << (7 - alive_count) << "\n";

        consecutive_dead = (alive_count == 0) ? consecutive_dead + 1 : 0;
        if (consecutive_dead >= 2) break;

        sleep(2);
    }

    std::cout << "\nИГРА ОКОНЧЕНА: два раунда подряд все мёртвы.\n";

    connections.clear();  // закрытие соединения и выход козлят

    for (int i = 00; i < 7; ++i) {
        int status;
        wait(&status);
    }

    std::cout << "Все процессы завершились чисто.\n";
    return 0;
}