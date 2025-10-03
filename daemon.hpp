#ifndef DAEMON_HPP
#define DAEMON_HPP

#include <string>

class Daemon {
private:
    static Daemon* instance; // Единственный экземпляр
    std::string dir1, dir2;  // Пути к папкам
    std::string config_path; // Путь к конфиг-файлу
    std::string pid_file;    // Путь к PID-файлу
    int interval;            // Интервал (секунды)

    Daemon(); // Приватный конструктор

public:
    static Daemon* getInstance();
    void daemonize();
    void readConfig();
    void run();
    static void handleSignal(int sig);
    void checkPidFile();
    void log(const std::string& msg, int level);
};

#endif