#pragma once
#include <string>
#include <concepts>
#include <map>
#include <type_traits>
#include "smart_ptr.hpp"
#include "task.hpp"
#include "logger.hpp"
#include "game_state.hpp"
#include "host.hpp"

struct PlayerBase; // forward

// Концепт для классов ролей
template<typename T>
concept PlayerRoleConcept = requires(
        T obj,
        GameState &gs,
        Host &host,
        int id,
        Logger& log,
        int round) {
    { obj.act(gs, host, id, log, round, true) } -> std::same_as<Task<>>;
    { obj.vote(gs, host, id, log, round) } -> std::same_as<Task<>>;
    // role() возвращает строку
    { obj.role() } -> std::convertible_to<std::string>;
    // методы для установки/получения цели
    { obj.set_target(0) };
    { obj.get_target() } -> std::convertible_to<int>;
    { obj.set_kill(0) };
};
