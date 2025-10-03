Lab1: Демон для копирования *.bk файлов
Демон (singleton класс `Daemon`): копирует `*.bk` файлы из `dir1` в `dir2`, очищая `dir2`; работает в фоне с `fork`, `setsid`, перенаправлением на `/dev/null`; читает `config.conf` (пути, интервал); обрабатывает SIGHUP (перечитка конфига), SIGTERM (завершение); логирует в `syslog` с `openlog`, `syslog`, `closelog`; защищён от повторного запуска через `/var/run/mydaemon.pid` и `/proc`.

Требования:
- Ubuntu (WSL2/Ubuntu 24.04)
- Пакеты: `g++`, `rsyslog`
- Права `sudo`

Файлы:
- `build.sh`: Сборка с `-Wall -Werror`
- `daemon.hpp`, `daemon.cpp`: Класс `Daemon`
- `main.cpp`: Точка входа
- `config.conf`: Конфигурация

Сборка:
cd lab1
./build.sh

Настройка:
sudo apt update
sudo apt install rsyslog -y
sudo rsyslogd -n &
mkdir ~/source ~/destination
echo "Test content" > ~/source/test.bk

Запуск:
sudo ./mydaemon &

Проверка:
1. Копирование:
ls ~/destination
cat ~/destination/test.bk
Ожидаемо: `test.bk`, содержимое: `Test content`

2. Очистка:
echo "Junk" > ~/destination/junk.txt
ls ~/destination
Ожидаемо: только `test.bk` через 30 сек

3. Логи:
sudo cat /var/log/syslog | grep mydaemon
Ожидаемо: `Daemon started`, `Config read`, `Copied`

4. Сигналы:
ps aux | grep mydaemon
sudo kill -SIGHUP <PID>
sudo kill -SIGTERM <PID>
Ожидаемо: `Received SIGHUP`, `Received SIGTERM`