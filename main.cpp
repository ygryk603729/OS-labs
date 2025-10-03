#include "daemon.hpp"

int main() {
    Daemon* daemon = Daemon::getInstance();
    daemon->daemonize();
    daemon->readConfig();
    daemon->run();
    return 0;
}