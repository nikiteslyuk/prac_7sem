#pragma once

#include <map>
#include <vector>
#include <string>
#include <random>
#include <optional>
#include <algorithm>
#include "smart_ptr.hpp"
#include "role_traits.hpp"

struct PlayerBase;

// Общая «точка истины» игры: живые игроки, RNG, настройки и утилиты.
struct GameState {
    // id -> игрок
    int human_id = -1;
    std::map<int, smart_ptr<PlayerBase>> alive;
    // исходные роли всех игроков (включая выбывших)
    std::map<int, RoleKind> original_roles;

    // конфигурация/состояние
    int round = 1;
    int k = 4;                 // N/k — число мафии
    bool announce_open = true; // режим объявлений
    bool human_enabled = false;

    // RNG
    std::mt19937 rng{std::random_device{}()};

    std::map<int, int> day_votes;      // кто за кого проголосовал (день)
    std::map<int, int> night_choices;  // ночные действия игрока -> цель

    // отладочные сведения о ночных действиях
    std::vector<int> mafia_last_votes;
    int mafia_final_target = -1;
    bool mafia_choice_random = false;

    void set_vote(int from, int to) {
        day_votes[from] = to; // to == -1 => воздержался
    }

    void set_night_choice(int from, int to) {
        night_choices[from] = to; // to == -1 => пас
    }

    // утилиты выбора
    int pick_random(const std::vector<int> &ids) {
        if (ids.empty()) return -1;
        std::uniform_int_distribution<int> d(0, (int) ids.size() - 1);
        return ids[d(rng)];
    }

    std::vector<int> ids() const {
        std::vector<int> v;
        v.reserve(alive.size());
        for (auto &[id, _]: alive) v.push_back(id);
        return v;
    }

    std::vector<int> ids_excluding(int self) const {
        std::vector<int> v;
        v.reserve(alive.size());
        for (auto &[id, _]: alive) if (id != self) v.push_back(id);
        return v;
    }

    // подсчёты по исходным ролям
    int mafia_count() const {
        int c = 0;
        for (const auto &[id, _] : alive) {
            auto it = original_roles.find(id);
            if (it != original_roles.end() && is_mafia_aligned(it->second)) {
                ++c;
            }
        }
        return c;
    }

    int maniac_count() const {
        int c = 0;
        for (const auto &[id, _] : alive) {
            auto it = original_roles.find(id);
            if (it != original_roles.end() && is_maniac(it->second)) {
                ++c;
            }
        }
        return c;
    }

    int civilians_count() const {
        int c = 0;
        for (const auto &[id, _] : alive) {
            auto it = original_roles.find(id);
            if (it != original_roles.end() && alignment_for(it->second) == Alignment::Civilian) {
                ++c;
            }
        }
        return c;
    }

    int witness_target = -1;
    int witness_observer = -1;
    std::optional<std::string> witness_message;
    int last_killer = -1;
};
