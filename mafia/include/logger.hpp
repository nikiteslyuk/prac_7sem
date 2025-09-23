#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

class Logger {
    std::filesystem::path log_dir;

    [[nodiscard]] std::filesystem::path day_file(int day) const;

public:
    Logger();

    void start_day(int day, const std::vector<int> &alive);
    void log_day_votes(int day, const std::map<int, int> &votes);
    void log_day_statement(int day, const std::string &line);
    void log_day_result(int day, std::optional<int> eliminated);
    void start_night(int day);
    void log_night_action(int day, const std::string &description);
    void log_final(const std::string &line);
    void log_game_set(const std::map<int, std::string> &roles);
};

