#pragma once

#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <vector>

class Logger {
    std::filesystem::path log_dir;

    [[nodiscard]] std::filesystem::path day_file(int day) const {
        return log_dir / ("day" + std::to_string(day));
    }

public:
    Logger() {
        log_dir = std::filesystem::current_path() / "logs";
        std::filesystem::create_directories(log_dir);
        for (auto &p : std::filesystem::directory_iterator(log_dir)) {
            if (p.is_regular_file()) {
                std::error_code ec;
                std::filesystem::remove(p.path(), ec);
            }
        }
    }

    void start_day(int day, const std::vector<int> &alive) {
        std::ofstream out(day_file(day), std::ios::trunc);
        out << "=== День " << day << " ===\n";
        out << "Живые: ";
        for (size_t i = 0; i < alive.size(); ++i) {
            if (i) out << ", ";
            out << alive[i];
        }
        out << "\n";
    }

    void log_day_votes(int day, const std::map<int, int> &votes) {
        std::ofstream out(day_file(day), std::ios::app);
        out << "Голоса:\n";
        if (votes.empty()) {
            out << "  нет голосов\n";
            return;
        }
        for (const auto &[from, to] : votes) {
            out << "  " << from << " -> ";
            if (to == -1) {
                out << "-1 (воздержался)";
            } else {
                out << to;
            }
            out << "\n";
        }
    }

    void log_day_statement(int day, const std::string &line) {
        std::ofstream out(day_file(day), std::ios::app);
        out << "[Обсуждение] " << line << '\n';
    }

    void log_day_result(int day, std::optional<int> eliminated) {
        std::ofstream out(day_file(day), std::ios::app);
        out << "Итог: ";
        if (eliminated.has_value()) {
            out << "Игрок " << *eliminated << " изгнан.";
        } else {
            out << "Никто не изгнан.";
        }
        out << "\n";
    }

    void start_night(int day) {
        std::ofstream out(day_file(day), std::ios::app);
        out << "=== Ночь " << day << " ===\n";
    }

    void log_night_action(int day, const std::string &description) {
        std::ofstream out(day_file(day), std::ios::app);
        out << description << '\n';
    }

    void log_final(const std::string &line) {
        std::ofstream out(log_dir / "game_result.log", std::ios::app);
        out << line << '\n';
    }

    void log_game_set(const std::map<int, std::string> &roles) {
        std::ofstream out(log_dir / "set_game", std::ios::trunc);
        for (const auto &[id, role] : roles) {
            out << id << ": " << role << '\n';
        }
    }
};
