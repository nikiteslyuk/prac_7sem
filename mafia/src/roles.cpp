#include "roles.hpp"
#include "host.hpp"

#include <algorithm>

static_assert(PlayerRoleConcept<Civilian>);
static_assert(PlayerRoleConcept<Mafia>);
static_assert(PlayerRoleConcept<Bull>);
static_assert(PlayerRoleConcept<Commissioner>);
static_assert(PlayerRoleConcept<Doctor>);
static_assert(PlayerRoleConcept<Maniac>);
static_assert(PlayerRoleConcept<Witness>);
static_assert(PlayerRoleConcept<Ninja>);

Task<> Civilian::act(GameState &, Host &, int, Logger &, int, bool) {
    co_return;
}

Task<> Civilian::vote(GameState &state, Host &, int id, Logger &, int) {
    auto ids = state.ids_excluding(id);
    target = ids.empty() ? -1 : choose_random(ids);
    co_return;
}

RoleKind Civilian::kind() const { return RoleKind::Civilian; }

void Civilian::set_target(int t) { target = t; }

int Civilian::get_target() const { return target; }

Task<> Mafia::act(GameState &, Host &host, int id, Logger &, int, bool mafiaPhase) {
    if (mafiaPhase) {
        auto ids = host.mafia_targets_for(id);
        target = ids.empty() ? -1 : choose_random(ids);
    }
    co_return;
}

Task<> Mafia::vote(GameState &state, Host &, int id, Logger &, int) {
    auto ids = state.ids_excluding(id);
    target = ids.empty() ? -1 : choose_random(ids);
    co_return;
}

RoleKind Mafia::kind() const { return RoleKind::Mafia; }

void Mafia::set_target(int t) { target = t; }

int Mafia::get_target() const { return target; }

Task<> Bull::act(GameState &, Host &host, int id, Logger &, int, bool mafiaPhase) {
    if (mafiaPhase) {
        auto ids = host.mafia_targets_for(id);
        target = ids.empty() ? -1 : choose_random(ids);
    }
    co_return;
}

Task<> Bull::vote(GameState &state, Host &, int id, Logger &, int) {
    auto ids = state.ids_excluding(id);
    target = ids.empty() ? -1 : choose_random(ids);
    co_return;
}

RoleKind Bull::kind() const { return RoleKind::Bull; }

void Bull::set_target(int t) { target = t; }

int Bull::get_target() const { return target; }

Task<> Commissioner::act(GameState &state, Host &host, int id, Logger &, int, bool mafiaPhase) {
    if (!mafiaPhase) {
        co_return;
    }

    if (kill_target != -1 && !state.alive.count(kill_target)) {
        kill_target = -1;
    }

    night_decision = NightDecision::None;
    inspected = -1;
    shot_target = -1;

    if (kill_target != -1) {
        night_decision = NightDecision::Shoot;
        shot_target = kill_target;
        co_return;
    }

    auto ids = host.alive_except(id);
    ids.erase(std::remove_if(ids.begin(), ids.end(), [&](int pid) {
        return known_civ.count(pid) != 0;
    }), ids.end());

    if (ids.empty()) {
        ids = host.alive_except(id);
    }

    if (!ids.empty()) {
        night_decision = NightDecision::Inspect;
        inspected = choose_random(ids);
    }

    co_return;
}

Task<> Commissioner::vote(GameState &, Host &, int, Logger &, int) {
    if (kill_target != -1) {
        target = kill_target;
    }
    co_return;
}

RoleKind Commissioner::kind() const { return RoleKind::Commissioner; }

void Commissioner::set_target(int t) {
    night_decision = NightDecision::None;
    inspected = -1;
    shot_target = -1;

    if (t == -1) {
        return;
    }

    if (t <= -2) {
        night_decision = NightDecision::Shoot;
        shot_target = -(t + 1);
        target = shot_target;
        return;
    }

    night_decision = NightDecision::Inspect;
    inspected = t;
}

void Commissioner::set_kill(int t) { kill_target = t; }

int Commissioner::get_target() const { return target; }

int Commissioner::night_action_target() const {
    return night_decision == NightDecision::Inspect ? inspected : -1;
}

int Commissioner::night_shot_target() const {
    return night_decision == NightDecision::Shoot ? shot_target : -1;
}

void Commissioner::apply_inspection_result(Host &host) {
    if (night_decision != NightDecision::Inspect || inspected == -1) {
        return;
    }

    auto align = host.alignment_of(inspected);
    const std::string inspected_role = host.role_name_for(inspected);
    if (inspected_role == "Ниндзя") {
        align = Host::Alignment::Civilian;
    }
    if (align == Host::Alignment::Mafia) {
        kill_target = inspected;
        known_civ.erase(inspected);
    } else if (align == Host::Alignment::Unknown) {
        // нет информации
    } else {
        known_civ.insert(inspected);
    }
}

Task<> Doctor::act(GameState &state, Host &, int, Logger &, int, bool) {
    auto ids = state.ids();
    if (!ids.empty()) {
        int who;
        do {
            who = choose_random(ids);
        } while (who == prev && ids.size() > 1);
        target = who;
        prev = who;
    }
    co_return;
}

Task<> Doctor::vote(GameState &state, Host &, int id, Logger &, int) {
    auto ids = state.ids_excluding(id);
    target = ids.empty() ? -1 : choose_random(ids);
    co_return;
}

RoleKind Doctor::kind() const { return RoleKind::Doctor; }

void Doctor::set_target(int t) {
    target = t;
    if (t != -1) {
        prev = t;
    }
}

int Doctor::get_target() const { return target; }

Task<> Maniac::act(GameState &state, Host &, int id, Logger &, int, bool mafiaPhase) {
    if (!mafiaPhase) {
        co_return;
    }
    auto ids = state.ids_excluding(id);
    target = ids.empty() ? -1 : choose_random(ids);
    co_return;
}

Task<> Maniac::vote(GameState &state, Host &, int id, Logger &, int) {
    auto ids = state.ids_excluding(id);
    target = ids.empty() ? -1 : choose_random(ids);
    co_return;
}

RoleKind Maniac::kind() const { return RoleKind::Maniac; }

void Maniac::set_target(int t) { target = t; }

int Maniac::get_target() const { return target; }
Task<> Witness::act(GameState &state, Host &host, int id, Logger &, int, bool mafiaPhase) {
    (void)host;
    if (!mafiaPhase) {
        co_return;
    }
    auto ids = state.ids_excluding(id);
    if (!ids.empty()) {
        observed = choose_random(ids);
        state.witness_target = observed;
        state.witness_observer = id;
    } else {
        observed = -1;
        state.witness_target = -1;
        state.witness_observer = -1;
    }
    co_return;
}

Task<> Witness::vote(GameState &state, Host &host, int id, Logger &, int) {
    (void)host;
    auto ids = state.ids_excluding(id);
    vote_choice = ids.empty() ? -1 : choose_random(ids);
    co_return;
}

RoleKind Witness::kind() const { return RoleKind::Witness; }

void Witness::set_target(int t) { observed = t; }

int Witness::get_target() const { return vote_choice; }

Task<> Ninja::act(GameState &state, Host &host, int id, Logger &, int, bool mafiaPhase) {
    if (!mafiaPhase) {
        co_return;
    }
    std::vector<int> ids;
    for (auto &[pid, _] : state.alive) {
        if (pid == id) continue;
        if (host.alignment_of(pid) != Host::Alignment::Mafia) ids.push_back(pid);
    }
    target = ids.empty() ? -1 : choose_random(ids);
    co_return;
}

Task<> Ninja::vote(GameState &state, Host &host, int id, Logger &, int) {
    (void)host;
    auto ids = state.ids_excluding(id);
    target = ids.empty() ? -1 : choose_random(ids);
    co_return;
}

RoleKind Ninja::kind() const { return RoleKind::Ninja; }

void Ninja::set_target(int t) { target = t; }

int Ninja::get_target() const { return target; }
