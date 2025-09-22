#include "host.hpp"
#include "roles.hpp"
#include "human.hpp"
#include <iostream>
#include <map>
#include <algorithm>
#include <optional>
#include <vector>

Host::Host(GameState &g, Logger &l) : gs(g), log(l) {
    gs.original_roles.clear();
    for (auto &[id, player] : gs.alive) {
        if (player.get()) {
            gs.original_roles[id] = player->role();
        }
    }
}

std::vector<int> Host::alive_ids() const {
    return gs.ids();
}

std::vector<int> Host::alive_except(int exclude_id) const {
    return gs.ids_excluding(exclude_id);
}

std::vector<int> Host::mafia_targets_for(int requester_id) const {
    std::vector<int> ids;
    ids.reserve(gs.alive.size());
    for (const auto &[pid, _] : gs.alive) {
        if (pid == requester_id) continue;
        auto align = alignment_of(pid);
        if (align != Alignment::Mafia) {
            ids.push_back(pid);
        }
    }
    return ids;
}

Host::Alignment Host::alignment_of(int id) const {
    auto it = gs.original_roles.find(id);
    if (it == gs.original_roles.end()) {
        return Alignment::Unknown;
    }
    const std::string &role = it->second;
    if (role == "Мафия" || role == "Дон") {
        return Alignment::Mafia;
    }
    if (role == "Маньяк") {
        return Alignment::Maniac;
    }
    return Alignment::Civilian;
}

std::string Host::alignment_to_string(Alignment a) const {
    switch (a) {
        case Alignment::Mafia:   return "мафию";
        case Alignment::Maniac:  return "маньяка";
        case Alignment::Civilian:return "мирного";
        default:                 return "неизвестного игрока";
    }
}

std::string Host::role_name_for(int id) const {
    auto it = gs.original_roles.find(id);
    if (it == gs.original_roles.end()) {
        return "";
    }
    return it->second;
}

void Host::day_discussion(const std::vector<int> &order) {
    static const std::vector<std::string> accusations = {"мафией", "маньяком"};
    if (accusations.empty()) return;

    for (int speaker : order) {
        if (!gs.alive.count(speaker)) continue;
        auto targets = gs.ids_excluding(speaker);
        if (targets.empty()) continue;
        int target = gs.pick_random(targets);
        if (target == -1) continue;
        std::uniform_int_distribution<size_t> dist(0, accusations.size() - 1);
        std::string accusation = accusations[dist(gs.rng)];
        std::string line = "Игрок " + std::to_string(speaker) + " считает, что игрок " +
                           std::to_string(target) + " является " + accusation + ".";
        announce(line);
        log.log_day_statement(gs.round, line);
    }
}

void Host::announce(const std::string &msg) {
    if (gs.announce_open) {
        std::cout << msg << std::endl;
    }
}

void Host::resolve_day() {
    log.log_day_votes(gs.round, gs.day_votes);

    std::map<int, int> votes;
    for (auto &[from, to] : gs.day_votes) {
        if (to != -1) votes[to]++;
    }
    if (votes.empty()) {
        announce("Днём никто не был изгнан.");
        log.log_day_result(gs.round, std::nullopt);
        gs.day_votes.clear();
        return;
    }
    auto best = std::max_element(votes.begin(), votes.end(),
                                 [](auto &a, auto &b) { return a.second < b.second; });
    int victim = best->first;
    std::cout << "\n[Сводка голосования дня]" << std::endl;
    for (auto &[from, to] : gs.day_votes) {
        std::cout << from << " → " << to << std::endl;
    }
    log.log_day_result(gs.round, victim);
    announce("Днём игрок " + std::to_string(victim) + " был изгнан.");
    gs.alive.erase(victim);
    gs.day_votes.clear();
}

void Host::resolve_night() {
    int mafia_target = -1;
    int maniac_target = -1;
    int doctor_target = -1;
    int commissioner_inspect_target = -1;
    int commissioner_shot_target = -1;
    int commissioner_id = -1;
    Commissioner *commissioner_ptr = nullptr;

    for (auto &[id, p] : gs.alive) {
        const std::string role = p->role();
        if (role == "Мафия") {
            if (mafia_target == -1) {
                mafia_target = p->get_target();
            }
        } else if (role == "Маньяк") {
            maniac_target = p->get_target();
        } else if (role == "Доктор") {
            doctor_target = p->get_target();
        } else if (role == "Комиссар") {
            commissioner_inspect_target = p->night_action_target();
            commissioner_shot_target = p->night_shot_target();
            commissioner_ptr = dynamic_cast<Commissioner *>(p.get());
            commissioner_id = id;
        }
    }

    bool doctor_saved_from_mafia = mafia_target != -1 && doctor_target == mafia_target;
    bool doctor_saved_from_maniac = maniac_target != -1 && doctor_target == maniac_target;
    bool doctor_saved_from_commissioner = commissioner_shot_target != -1 && doctor_target == commissioner_shot_target;
    bool doctor_saved_anyone = doctor_target != -1 && (doctor_saved_from_mafia || doctor_saved_from_maniac || doctor_saved_from_commissioner);

    if (mafia_target == -1) {
        log.log_night_action(gs.round, "Мафия завершила ночь без действия.");
    } else {
        log.log_night_action(gs.round, doctor_saved_from_mafia ? "Мафия выполнила ход, цель спасена." : "Мафия выполнила ход.");
    }

    if (doctor_target == -1) {
        log.log_night_action(gs.round, "Доктор выполнил ход.");
    } else if (doctor_saved_anyone) {
        log.log_night_action(gs.round, "Доктор спас игрока.");
    } else {
        log.log_night_action(gs.round, "Доктор вылечил не того.");
    }

    if (commissioner_ptr) {
        if (commissioner_shot_target != -1) {
            log.log_night_action(gs.round, "Комиссар выполнил выстрел.");
            if (gs.human_enabled && gs.human_id == commissioner_id) {
                std::cout << "[Комиссар] Выстрел по цели " << commissioner_shot_target
                          << (doctor_saved_from_commissioner ? " — цель спасена." : " — цель поражена.")
                          << '\n';
            }
        } else if (commissioner_inspect_target == -1) {
            log.log_night_action(gs.round, "Комиссар выполнил ход.");
            if (gs.human_enabled && gs.human_id == commissioner_id) {
                std::cout << "[Комиссар] Проверка не состоялась.\n";
            }
        } else {
            commissioner_ptr->apply_inspection_result(*this);
            auto align = alignment_of(commissioner_inspect_target);
            if (align == Alignment::Unknown) {
                log.log_night_action(gs.round, "Комиссар выполнил ход.");
                if (gs.human_enabled && gs.human_id == commissioner_id) {
                    std::cout << "[Комиссар] Не удалось получить результат проверки." << '\n';
                }
            } else {
                Alignment report = (align == Alignment::Maniac) ? Alignment::Civilian : align;
                log.log_night_action(gs.round, "Комиссар выполнил проверку.");
                if (gs.human_enabled && gs.human_id == commissioner_id) {
                    std::cout << "[Комиссар] Результат проверки: "
                              << (report == Alignment::Mafia ? "мафия" : "мирный")
                              << " (" << commissioner_inspect_target << ")" << '\n';
                }
            }
        }
    } else {
        log.log_night_action(gs.round, "Комиссар выполнил ход.");
    }

    auto kill = [&](int vid, const std::string &by) {
        if (vid == -1) return;
        if (vid == doctor_target) {
            announce("Доктор спас игрока " + std::to_string(vid) + " от " + by);
            return;
        }
        if (gs.alive.count(vid)) {
            announce("Игрок " + std::to_string(vid) + " погиб от " + by);
            log.log_night_action(gs.round, "Игрок " + std::to_string(vid) + " погиб от " + by + '.');
            gs.alive.erase(vid);
        }
    };

    kill(mafia_target, "мафии");
    kill(maniac_target, "маньяка");
    kill(commissioner_shot_target, "комиссара");
}


bool Host::game_over() {
    int m = gs.mafia_count();
    int civ = gs.civilians_count();
    int man = gs.maniac_count();

    if (m == 0 && man == 0) {
        announce("Победили мирные жители!");
        return true;
    }
    if (m >= civ) {
        announce("Победила мафия!");
        return true;
    }
    if (man == 1 && civ == 1 && m == 0) {
        announce("Маньяк победил!");
        return true;
    }
    return false;
}

Task<> Host::run() {
    for (gs.round = 1;; ++gs.round) {
        announce("Раунд " + std::to_string(gs.round));
        std::vector<int> ids;
        ids.reserve(gs.alive.size());
        for (auto &[id, _] : gs.alive) ids.push_back(id);
        log.start_day(gs.round, ids);

        // === День: каждый игрок голосует ===
        gs.day_votes.clear();
        {
            std::vector<int> ids_local;
            ids_local.reserve(gs.alive.size());
            for (auto &[id, _] : gs.alive) ids_local.push_back(id);

            day_discussion(ids_local);

            for (int id : ids_local) {
                auto it = gs.alive.find(id);
                if (it == gs.alive.end()) continue;
                auto &p = it->second;
                if (gs.human_enabled && id == gs.human_id) {
                    co_await human_loop(gs, *this, id, log, /*day_phase=*/true);
                } else {
                    co_await p->vote(gs, *this, id, log, gs.round);
                    gs.set_vote(id, p->get_target());
                }
            }
        }
        resolve_day();
        if (game_over()) co_return;

        log.start_night(gs.round);
        // === Ночь: действия ролей ===
        gs.night_choices.clear();
        {
            std::vector<int> ids_local;
            ids_local.reserve(gs.alive.size());
            for (auto &[id, _] : gs.alive) ids_local.push_back(id);

            bool human_is_mafia = false;
            if (gs.human_enabled && gs.alive.count(gs.human_id)) {
                human_is_mafia = (alignment_of(gs.human_id) == Alignment::Mafia);
            }

            for (int id : ids_local) {
                auto it = gs.alive.find(id);
                if (it == gs.alive.end()) continue;
                auto &p = it->second;
                if (gs.human_enabled && id == gs.human_id) continue;
                co_await p->act(gs, *this, id, log, gs.round, /*mafiaPhase=*/true);
            }

            if (human_is_mafia) {
                std::vector<int> other_mafia_ids;
                for (auto &[id, _] : gs.alive) {
                    if (id == gs.human_id) continue;
                    if (alignment_of(id) == Alignment::Mafia) other_mafia_ids.push_back(id);
                }
                if (!other_mafia_ids.empty()) {
                    std::cout << "\n[Подсказка] Выбор других мафий: ";
                    bool first = true;
                    for (int oid : other_mafia_ids) {
                        int t = -1;
                        auto it = gs.alive.find(oid);
                        if (it != gs.alive.end()) t = it->second->get_target();
                        if (!first) std::cout << ", ";
                        std::cout << oid << "→" << t;
                        first = false;
                    }
                    std::cout << "\n";
                }
            }

            if (gs.human_enabled && gs.alive.count(gs.human_id)) {
                co_await human_loop(gs, *this, gs.human_id, log, /*day_phase=*/false);
            }

            for (auto &[from, to] : gs.night_choices) {
                auto itp = gs.alive.find(from);
                if (itp != gs.alive.end()) itp->second->set_target(to);
            }

            {
                std::vector<int> mafia_ids;
                for (auto &[id, _] : gs.alive) if (alignment_of(id) == Alignment::Mafia) mafia_ids.push_back(id);
                if (mafia_ids.size() >= 2) {
                    std::vector<int> chosen;
                    for (int id : mafia_ids) {
                        auto &p = gs.alive.at(id);
                        if (p->get_target() != -1) chosen.push_back(p->get_target());
                    }
                    if (!chosen.empty()) {
                        std::map<int, int> cnt;
                        int bestc = 0;
                        for (int t : chosen) {
                            auto &ref = cnt[t];
                            ++ref;
                            if (ref > bestc) bestc = ref;
                        }

                        std::vector<int> max_candidates;
                        max_candidates.reserve(chosen.size());
                        for (int t : chosen) {
                            if (cnt[t] == bestc) max_candidates.push_back(t);
                        }

                        int final_target = -1;
                        if (!max_candidates.empty()) {
                            if (max_candidates.size() == 1) {
                                final_target = max_candidates.front();
                            } else {
                                std::uniform_int_distribution<int> d(0, (int)max_candidates.size() - 1);
                                final_target = max_candidates[d(gs.rng)];
                            }

                            int unique_max = 0;
                            for (auto &[t, c] : cnt) if (c == bestc) ++unique_max;
                            if (human_is_mafia && unique_max > 1) {
                                announce("Голоса мафии разделились. Случайно выбрана цель " + std::to_string(final_target));
                            }
                        }

                        if (final_target != -1) {
                            for (int id : mafia_ids) gs.alive.at(id)->set_target(final_target);
                        }
                    }
                }
            }

            for (int id : ids_local) {
                auto it = gs.alive.find(id);
                if (it == gs.alive.end()) continue;
                auto &p = it->second;
                if (alignment_of(id) == Alignment::Mafia) continue;
                co_await p->act(gs, *this, id, log, gs.round, /*mafiaPhase=*/false);
            }
        }
        resolve_night();
        if (game_over()) co_return;
    }
}
