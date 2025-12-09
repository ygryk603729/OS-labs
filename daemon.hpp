#pragma once
#include <string>

class Daemon {
private:
    static Daemon* instance;
    std::string dir1, dir2, config_path, pid_file;
    int interval;

    Daemon();
    Daemon(const Daemon&) = delete;
    Daemon& operator=(const Daemon&) = delete;

    void daemonize();
    void readConfig();
    void run();
    void clearDir2();
    void copyBkFiles();
    static void handleSignal(int sig);
    void checkPidFile();
    void log(const std::string& msg, int level);
    void ensureDirExists(const std::string& path);
public:
    static Daemon* getInstance();
    static void start();
};