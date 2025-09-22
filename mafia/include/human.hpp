#pragma once

#include <iostream>
#include <string>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <future>
#include <thread>
#include <chrono>

#include "task.hpp"
#include "game_state.hpp"
#include "signals.hpp"
#include "logger.hpp"
#include "host.hpp"

// awaiter для std::future<T>
template<typename T>
struct FutureAwaiter {
    std::future<T> fut;

    bool await_ready() const {
        using namespace std::chrono_literals;
        return fut.wait_for(0s) == std::future_status::ready;
    }

    void await_suspend(std::coroutine_handle<> h) {
        std::thread([h, f = std::move(fut)]() mutable {
            f.wait();
            h.resume();
        }).detach();
    }

    T await_resume() { return fut.get(); }
};

inline Task<std::string> async_input(const std::string &prompt) {
    // Синхронный ввод без фоновых потоков — безопасно для ручного resume()
    std::cout << prompt << std::flush;
    std::string s;
    if (std::cin.peek() == '\n') std::cin.get();
    std::getline(std::cin, s);
    co_return s;
}

// корутина интерактивного игрока
inline Task<> human_loop(GameState &gs,
                         Host &host,
                         int human_id,
                         Logger &log,
                         bool day_phase) {
    (void)log;
    if (day_phase) {
        // голосование
        std::cout << "\n=== День — Раунд " << gs.round << " ===\n";
        {
            auto it = gs.alive.find(human_id);
            std::string my_role = host.role_name_for(human_id);
            if (my_role.empty()) my_role = "?";
            std::cout << "Ваш id: " << human_id << " (" << (it != gs.alive.end() ? my_role : std::string("вне игры")) << ")\n";
        }
        std::cout << "Живые игроки: ";
        {
            bool first = true;
            for (auto &[id, _] : gs.alive) {
                if (!first) std::cout << ", ";
                std::cout << id;
                first = false;
            }
            std::cout << "\n";
        }
        std::cout << "День: выберите действие:\n";
        std::cout << "  • Проголосовать против игрока — введите его id\n";
        std::cout << "  • Воздержаться — введите -1\n";
        std::cout << "Подсказка: голосовать за себя нельзя.\n";
        std::string sv = co_await async_input("[День] Ваш выбор (id или -1): ");
        int vote_id = -1;
        if (!sv.empty()) {
            try { vote_id = std::stoi(sv); } catch (...) { vote_id = -1; }
        }

        if (gs.alive.contains(vote_id) && vote_id != human_id) {
            gs.set_vote(human_id, vote_id);
        } else {
            gs.set_vote(human_id, -1);
        }
        co_return;
    } else {
        // ночь: действие зависит от роли
        auto role = host.role_name_for(human_id);

        if (role == "Мирный") {
            std::cout << "\n=== Ночь — Раунд " << gs.round << " ===\n";
            std::cout << "Вы — мирный житель. Ночью действий нет, вы спите. Нажмите Enter.\n";
            co_return;
        }

        if (role == "Комиссар") {
            std::cout << "\n=== Ночь — Раунд " << gs.round << " ===\n";
            std::cout << "Вы — Комиссар. Ночь: выберите действие:\n";
            std::cout << "  • Проверить игрока — введите 'p <id>'\n";
            std::cout << "  • Выстрелить в игрока — введите 's <id>'\n";
            std::cout << "Пропустить ход нельзя.\n";
            std::cout << "Живые (кроме вас): ";
            {
                bool first = true;
                for (auto &[id, _] : gs.alive) if (id != human_id) {
                    if (!first) std::cout << ", ";
                    std::cout << id;
                    first = false;
                }
                std::cout << "\n";
            }
            std::string s = co_await async_input("[Ночь/Комиссар] команда: ");
            int t = -1;
            bool shoot = false;
            if (!s.empty()) {
                std::istringstream iss(s);
                std::string token;
                iss >> token;
                if (!token.empty()) {
                    std::string lower = token;
                    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
                    if (lower == "s" || lower == "shoot") {
                        shoot = true;
                    } else if (lower == "p" || lower == "check") {
                        // остаёмся в режиме проверки
                    } else {
                        try {
                            t = std::stoi(token);
                        } catch (...) {
                            token.clear();
                        }
                    }

                    if (t == -1 && (lower == "s" || lower == "shoot" || lower == "p" || lower == "check")) {
                        iss >> t;
                    }
                }

                if (t == -1 && iss && !iss.eof()) {
                    iss >> t;
                }

                if (t <= 0 || !gs.alive.contains(t) || t == human_id) {
                    t = -1;
                }
            }

            if (shoot && t != -1) {
                gs.set_night_choice(human_id, -(t + 1));
            } else if (t != -1) {
                gs.set_night_choice(human_id, t);
            } else {
                gs.set_night_choice(human_id, -1);
            }
            co_return;
        }

        if (role == "Доктор") {
            std::cout << "\n=== Ночь — Раунд " << gs.round << " ===\n";
            std::cout << "Вы — Доктор. Ночь: выберите действие (нельзя лечить одного и того же два раза подряд):\n";
            std::cout << "  • Лечить игрока — введите его id\n";
            std::cout << "Живые: ";
            {
                bool first = true;
                for (auto &[id, _] : gs.alive) {
                    if (!first) std::cout << ", ";
                    std::cout << id;
                    first = false;
                }
                std::cout << "\n";
            }
            int t = -1;
            while (true) {
                std::string s = co_await async_input("[Ночь/Доктор] id для лечения: ");
                if (!s.empty()) {
                    try { t = std::stoi(s); } catch (...) { t = -1; }
                }
                if (gs.alive.contains(t)) break;
                std::cout << "Доктор обязан выбрать одного из живых игроков. Попробуйте снова.\n";
            }
            gs.set_night_choice(human_id, t);
            co_return;
        }

        if (role == "Мафия") {
            std::cout << "\n=== Ночь — Раунд " << gs.round << " ===\n";
            std::cout << "Вы — Мафия. Ночь: выберите действие:\n";
            std::cout << "  • Выбрать жертву — введите её id\n";
            std::cout << "  • Пропустить ход — введите -1\n";
            std::cout << "Живые (кроме мафии): ";
            {
                bool first = true;
                for (auto &[id, _] : gs.alive) {
                    if (host.alignment_of(id) == Host::Alignment::Mafia) continue;
                    if (!first) std::cout << ", ";
                    std::cout << id;
                    first = false;
                }
                std::cout << "\n";
            }
            std::string s = co_await async_input("[Ночь/Мафия] id жертвы (или -1 чтобы пропустить): ");
            int t = -1;
            if (!s.empty()) {
                try { t = std::stoi(s); } catch (...) { t = -1; }
            }
            gs.set_night_choice(human_id, t);
            co_return;
        }

        // на всякий — если роль неизвестна
        std::cout << "\n=== Ночь — Раунд " << gs.round << " ===\n";
        std::cout << "Введите id цели (или -1, чтобы пропустить).\n";
        std::string s = co_await async_input("[Ночь] Ваш выбор: ");
        int t = -1;
        if (!s.empty()) {
            try { t = std::stoi(s); } catch (...) { t = -1; }
        }
        gs.set_night_choice(human_id, t);
        co_return;
    }
}
