#include "daemon.hpp"
#include <iostream>
#include <fstream>
#include <signal.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstdlib>
#include <climits>
#include <dirent.h>
#include <cstring>
#include <libgen.h>
#include <memory>

Daemon* Daemon::instance = nullptr;

Daemon::Daemon() : dir1("/home/user/source"), dir2("/home/user/destination"), config_path("/home/user/daemon_lab/config.conf"), pid_file("/var/run/mydaemon.pid"), interval(30) {}

Daemon* Daemon::getInstance() {
    if (!instance) {
        instance = new Daemon();
        signal(SIGTERM, handleSignal);
    }
    return instance;
}

void Daemon::start() {
    Daemon* daemon = getInstance();
    daemon->daemonize();
    daemon->readConfig();
    daemon->run();
}

void Daemon::daemonize() {
    pid_t pid = fork();
    if (pid < 0) { log("Fork failed", LOG_ERR); exit(EXIT_FAILURE); }
    if (pid > 0) exit(EXIT_SUCCESS);
    setsid();
    pid = fork();
    if (pid < 0) { log("Second fork failed", LOG_ERR); exit(EXIT_FAILURE); }
    if (pid > 0) exit(EXIT_SUCCESS);
    umask(0);
    chdir("/");
    for (int i = sysconf(_SC_OPEN_MAX); i >= 0; i--) close(i);
    open("/dev/null", O_RDWR);
    dup(0);
    dup(0);
    log("Daemon started", LOG_INFO);
}

void Daemon::clearDir2() {
    DIR* dir = opendir(dir2.c_str());
    if (!dir) {
        log("Failed to open dir2: " + dir2, LOG_ERR);
        return;
    }
    struct dirent* entry;
    while ((entry = readdir(dir))) {
        if (entry->d_type == DT_REG || entry->d_type == DT_DIR) {
            std::string path = dir2 + "/" + entry->d_name;
            if (std::strcmp(entry->d_name, ".") != 0 && std::strcmp(entry->d_name, "..") != 0) {
                if (remove(path.c_str()) != 0) {
                    log("Failed to delete: " + path, LOG_ERR);
                }
            }
        }
    }
    closedir(dir);
}

void Daemon::copyBkFiles() {
    DIR* dir = opendir(dir1.c_str());
    if (!dir) {
        log("Failed to open dir1: " + dir1, LOG_ERR);
        return;
    }
    struct dirent* entry;
    while ((entry = readdir(dir))) {
        if (entry->d_type == DT_REG) {
            std::string filename = entry->d_name;
            if (filename.length() >= 3 && filename.substr(filename.length() - 3) == ".bk") {
                std::string src = dir1 + "/" + filename;
                std::string dst = dir2 + "/" + filename;
                std::ifstream srcFile(src, std::ios::binary);
                std::ofstream dstFile(dst, std::ios::binary);
                if (srcFile && dstFile) {
                    dstFile << srcFile.rdbuf();
                    log("Copied: " + src + " to " + dst, LOG_INFO);
                } else {
                    log("Failed to copy: " + src, LOG_ERR);
                }
                srcFile.close();
                dstFile.close();
            }
        }
    }
    closedir(dir);
}

void Daemon::readConfig() {
    char abs_path[PATH_MAX];
    if (realpath(config_path.c_str(), abs_path) == nullptr) {
        log("Failed to resolve config path", LOG_ERR);
        return;
    }
    config_path = abs_path;
    std::ifstream configFile(config_path);
    if (!configFile.is_open()) {
        log("Failed to open config file", LOG_ERR);
        return;
    }
    std::string line;
    while (std::getline(configFile, line)) {
        if (line.find("dir1=") == 0) {
            dir1 = line.substr(5);
        }
        else if (line.find("dir2=") == 0) {
            dir2 = line.substr(5);
        }
        else if (line.find("interval=") == 0) {
            interval = std::stoi(line.substr(9));
        }
    }
    configFile.close();
    log("Config read: dir1=" + dir1 + ", dir2=" + dir2 + ", interval=" + std::to_string(interval), LOG_INFO);
}

void Daemon::run() {
    signal(SIGHUP, handleSignal);
    while (true) {
        clearDir2();
        copyBkFiles();
        sleep(interval);
    }
}

void Daemon::handleSignal(int sig) {
    Daemon* daemon = getInstance();
    if (sig == SIGHUP) {
        daemon->log("Received SIGHUP, reloading config", LOG_INFO);
        daemon->readConfig();
    } else if (sig == SIGTERM) {
        daemon->log("Received SIGTERM, exiting", LOG_INFO);
        delete instance;
        instance = nullptr;
        closelog();
        exit(EXIT_SUCCESS);
    }
}

void Daemon::checkPidFile() {
    std::ifstream pidFile(pid_file);
    if (pidFile.is_open()) {
        pid_t oldPid;
        pidFile >> oldPid;
        pidFile.close();
        std::string procPath = "/proc/" + std::to_string(oldPid);
        if (access(procPath.c_str(), F_OK) == 0) {
            log("Terminating existing daemon with PID " + std::to_string(oldPid), LOG_INFO);
            kill(oldPid, SIGTERM);
            sleep(1);
        }
    } else {
        log("No existing PID file found", LOG_INFO);
    }
    std::ofstream newPidFile(pid_file);
    if (!newPidFile.is_open()) {
        log("Failed to write PID file: " + pid_file, LOG_ERR);
        exit(EXIT_FAILURE);
    }
    newPidFile << getpid();
    newPidFile.close();
    log("PID file created with PID " + std::to_string(getpid()), LOG_INFO);
}

void Daemon::log(const std::string& msg, int level) {
    static bool syslog_opened = false;
    if (!syslog_opened) {
        openlog("mydaemon", LOG_PID, LOG_DAEMON);
        syslog_opened = true;
    }
    syslog(level, "%s", msg.c_str());
}