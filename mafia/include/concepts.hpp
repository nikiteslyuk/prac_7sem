#pragma once
#include <string>
#include <map>
#include "smart_ptr.hpp"
#include "task.hpp"
#include "logger.hpp"

struct PlayerBase; // forward

// Концепт для классов ролей
template<typename T>
concept PlayerRoleConcept = requires(
        T obj,
        std::map<int, smart_ptr<PlayerBase>>& alive,
        int id,
        Logger& log,
        int round) {
    // методы act/vote возвращают Task<>
    { obj.act(alive, id, log, round, true) } -> std::same_as<Task<>>;
    { obj.vote(alive, id, log, round) } -> std::same_as<Task<>>;
    // role() возвращает строку
    { obj.role() } -> std::convertible_to<std::string>;
    // методы для установки/получения цели
    { obj.set_target(0) };
    { obj.get_target() } -> std::convertible_to<int>;
    { obj.set_kill(0) };
};
