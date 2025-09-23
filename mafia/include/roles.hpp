#pragma once

#include <string>
#include <map>
#include <set>
#include <vector>
#include <random>
#include <utility>

#include "smart_ptr.hpp"
#include "task.hpp"
#include "logger.hpp"
#include "concepts.hpp"
#include "game_state.hpp"

struct Host;

struct PlayerBase {
    virtual ~PlayerBase() = default;

    virtual Task<> act(GameState &state,
                       Host &host,
                       int id,
                       Logger &log,
                       int round,
                       bool mafiaPhase) = 0;

    virtual Task<> vote(GameState &state,
                        Host &host,
                        int id,
                        Logger &log,
                        int round) = 0;

    virtual void set_target(int) {}
    virtual void set_kill(int) {}
    virtual int get_target() const { return -1; }
    virtual int night_action_target() const { return get_target(); }

    virtual int night_shot_target() const { return -1; }

protected:
    virtual std::string role() const = 0;

    friend struct Host;
    friend struct GameState;
};

// Утилита для создания роли с проверкой концепта
template<PlayerRoleConcept Role, class... Args>
smart_ptr<PlayerBase> make_role(Args&&... args) {
    return smart_ptr<PlayerBase>(new Role(std::forward<Args>(args)...));
}

// утилита для случайного выбора
template<class T>
T choose_random(const std::vector<T> &v) {
    if (v.empty()) return T{};
    static thread_local std::mt19937 gen{std::random_device{}()};
    std::uniform_int_distribution<size_t> dist(0, v.size() - 1);
    return v[dist(gen)];
}

// ===== Мирный =====
struct Civilian : PlayerBase {
    int target = -1;

    Task<> act(GameState &state,
               Host &host,
               int id,
               Logger &log,
               int round,
               bool mafiaPhase) override;

    Task<> vote(GameState &state,
                Host &host,
                int id,
                Logger &log,
                int round) override;

    std::string role() const override;

    void set_target(int t) override;
    int get_target() const override;
};

// ===== Мафия =====
struct Mafia : PlayerBase {
    int target = -1;

    Task<> act(GameState &state,
               Host &host,
               int id,
               Logger &log,
               int round,
               bool mafiaPhase) override;

    Task<> vote(GameState &state,
                Host &host,
                int id,
                Logger &log,
                int round) override;

    std::string role() const override;

    void set_target(int t) override;
    int get_target() const override;
};

// ===== Бык =====
struct Bull : PlayerBase {
    int target = -1;

    Task<> act(GameState &state,
               Host &host,
               int id,
               Logger &log,
               int round,
               bool mafiaPhase) override;

    Task<> vote(GameState &state,
                Host &host,
                int id,
                Logger &log,
                int round) override;

    std::string role() const override;

    void set_target(int t) override;
    int get_target() const override;
};

// ===== Комиссар =====
struct Commissioner : PlayerBase {
    enum class NightDecision { None, Inspect, Shoot };

    int target = -1;
    int kill_target = -1;
    int inspected = -1;
    int shot_target = -1;
    std::set<int> known_civ;
    NightDecision night_decision = NightDecision::None;

    Task<> act(GameState &state,
               Host &host,
               int id,
               Logger &log,
               int round,
               bool mafiaPhase) override;

    Task<> vote(GameState &state,
                Host &host,
                int id,
                Logger &log,
                int round) override;

    std::string role() const override;

    void set_target(int t) override;
    void set_kill(int t) override;
    int get_target() const override;
    int night_action_target() const override;
    int night_shot_target() const override;

    void apply_inspection_result(Host &host);
};

// ===== Доктор =====
struct Doctor : PlayerBase {
    int target = -1;
    int prev = -1;

    Task<> act(GameState &state,
               Host &host,
               int id,
               Logger &log,
               int round,
               bool mafiaPhase) override;

    Task<> vote(GameState &state,
                Host &host,
                int id,
                Logger &log,
                int round) override;

    std::string role() const override;

    void set_target(int t) override;
    int get_target() const override;
};

// ===== Маньяк =====
struct Maniac : PlayerBase {
    int target = -1;

    Task<> act(GameState &state,
               Host &host,
               int id,
               Logger &log,
               int round,
               bool mafiaPhase) override;

    Task<> vote(GameState &state,
                Host &host,
                int id,
                Logger &log,
                int round) override;

    std::string role() const override;

    void set_target(int t) override;
    int get_target() const override;
};

// ===== Свидетель =====
struct Witness : PlayerBase {
    int observed = -1;
    int vote_choice = -1;

    Task<> act(GameState &state,
               Host &host,
               int id,
               Logger &log,
               int round,
               bool mafiaPhase) override;

    Task<> vote(GameState &state,
                Host &host,
                int id,
                Logger &log,
                int round) override;

    std::string role() const override;

    void set_target(int t) override;
    int get_target() const override;
};

// ===== Ниндзя =====
struct Ninja : PlayerBase {
    int target = -1;

    Task<> act(GameState &state,
               Host &host,
               int id,
               Logger &log,
               int round,
               bool mafiaPhase) override;

    Task<> vote(GameState &state,
                Host &host,
                int id,
                Logger &log,
                int round) override;

    std::string role() const override;

    void set_target(int t) override;
    int get_target() const override;
};
