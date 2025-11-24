#pragma once

#include <string>
#include <cstddef>

enum class GoatStatus { ALIVE, DEAD };

struct Message {
    int wolf_number = 0;
    int goat_number = 0;
    GoatStatus status = GoatStatus::ALIVE;
    int goat_id = -1;
};

class Conn {
public:
    virtual ~Conn() = default;

    // Читает один Message, таймаут 5 сек
    // Возвращает false при таймауте или ошибке
    virtual bool read(Message& msg) = 0;

    // Пишет один Message
    // Возвращает false при ошибке
    virtual bool write(const Message& msg) = 0;

    // Для логов и отладки
    virtual std::string name() const = 0;
};