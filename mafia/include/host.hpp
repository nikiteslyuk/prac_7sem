#pragma once

#include <string>
#include <vector>
#include "signals.hpp"
#include "logger.hpp"
#include "game_state.hpp"
#include "task.hpp"

// Оркестратор игры: День/Ночь, сводка действий, условия завершения.
struct Host {
    enum class Alignment {
        Unknown,
        Civilian,
        Mafia,
        Maniac
    };

    GameState &gs;
    Logger &log;
    PhaseSignal day_sig{};
    PhaseSignal night_sig{};

    Host(GameState &g, Logger &l);

    // Главный цикл (корутина): День -> Ночь -> ...
    Task<> run();

    // Сведение дневных голосов (все роли уже выставили voted)
    void resolve_day();

    // Применение ночных действий (все роли уже выставили night.{shot/heal/check})
    void resolve_night();

    // Проверка победы сторон
    bool game_over();

    // Локальная запись событий раунда
    void announce(const std::string &msg);

    // ===== Служебные методы ведущего =====
    [[nodiscard]] std::vector<int> alive_ids() const;
    [[nodiscard]] std::vector<int> alive_except(int exclude_id) const;
    [[nodiscard]] std::vector<int> mafia_targets_for(int requester_id) const;
    [[nodiscard]] Alignment alignment_of(int id) const;
    [[nodiscard]] std::string alignment_to_string(Alignment a) const;
    [[nodiscard]] std::string role_name_for(int id) const;

    void day_discussion(const std::vector<int> &order);
};
