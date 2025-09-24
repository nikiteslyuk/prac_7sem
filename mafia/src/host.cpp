#include "host.hpp"
#include "roles.hpp"
#include "human.hpp"
#include <iostream>
#include <map>
#include <algorithm>
#include <optional>
#include <vector>
#include <ranges>

Host::Host(GameState &g, Logger &l) : gs(g), log(l) {
    gs.original_roles.clear();
    for (auto &[id, player] : gs.alive) {
        if (player.get()) {
            gs.original_roles[id] = player->kind();
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
    return alignment_for(it->second);
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
    return role_name(it->second);
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
    announce("\n=== Обсуждение — Раунд " + std::to_string(gs.round) + " ===");
    for (auto &[from, to] : gs.day_votes) {
        std::string line;
        if (to == -1) {
            line = "Игрок " + std::to_string(from) + " воздержался от голосования.";
        } else {
            std::string accusation_role = "мафией";
            auto it_role = gs.original_roles.find(to);
            if (it_role != gs.original_roles.end() && is_maniac(it_role->second)) {
                accusation_role = "маньяком";
            }
            line = "Игрок " + std::to_string(from) + " считает, что игрок " + std::to_string(to) + " является " + accusation_role + ".";
        }
        announce(line);
        log.log_day_statement(gs.round, line);
    }
    log.log_day_result(gs.round, victim);
    announce("\n--- Итоги дня (Раунд " + std::to_string(gs.round) + ") ---");
    announce("Голосованием изгнан игрок " + std::to_string(victim) + ".");
    gs.alive.erase(victim);
    gs.day_votes.clear();
}

// запихнуть все проверки в объекты
void Host::resolve_night() {
    announce("\n=== Ночь — Раунд " + std::to_string(gs.round) + " ===");

    int mafia_target = -1;
    int maniac_target = -1;
    int maniac_id = -1;
    int doctor_target = -1;
    std::vector<int> doctor_ids;

    int commissioner_inspect_target = -1;
    int commissioner_shot_target = -1;
    Commissioner *commissioner_ptr = nullptr;

    bool human_is_doctor = gs.human_enabled && gs.alive.count(gs.human_id) && gs.alive.at(gs.human_id)->is_doctor();
    bool human_is_commissioner = gs.human_enabled && gs.alive.count(gs.human_id) && gs.alive.at(gs.human_id)->is_commissioner();

    bool doctor_saved_someone = false;
    int doctor_saved_player = -1;
    std::string doctor_saved_by;
    bool doctor_died = false;

    // свернуть (полиморфизм)
    for (auto &[id, p] : gs.alive) {
        if (p->mafia_aligned()) {
            if (mafia_target == -1) {
                mafia_target = p->get_target();
            }
        }
        if (p->maniac_aligned()) {
            maniac_target = p->get_target();
            maniac_id = id;
        }
        if (p->is_doctor()) {
            doctor_ids.push_back(id);
        }
        if (p->is_commissioner()) {
            commissioner_inspect_target = p->night_action_target();
            commissioner_shot_target = p->night_shot_target();
            commissioner_ptr = dynamic_cast<Commissioner *>(p.get());
        }
    }

    if (!doctor_ids.empty()) {
        std::map<int, int> heal_votes;
        for (int did : doctor_ids) {
            auto it = gs.alive.find(did);
            if (it == gs.alive.end()) continue;
            int tgt = it->second->get_target();
            if (tgt == -1) continue;
            ++heal_votes[tgt];
        }

        if (!heal_votes.empty()) {
            int best_count = 0;
            std::vector<int> candidates;
            candidates.reserve(heal_votes.size());
            for (auto &[pid, count] : heal_votes) {
                if (count > best_count) {
                    best_count = count;
                    candidates.clear();
                    candidates.push_back(pid);
                } else if (count == best_count) {
                    candidates.push_back(pid);
                }
            }

            if (!candidates.empty()) {
                if (candidates.size() == 1) {
                    doctor_target = candidates.front();
                } else {
                    std::uniform_int_distribution<size_t> dist(0, candidates.size() - 1);
                    doctor_target = candidates[dist(gs.rng)];
                }
            }
        }

        std::string attempt = "Доктор, попытка лечения: ";
        if (doctor_target == -1) {
            attempt += "без цели.";
        } else {
            attempt += "игрок " + std::to_string(doctor_target) + ".";
        }
        log.log_night_action(gs.round, attempt);
    }

    if (gs.mafia_final_target == -1) {
        gs.mafia_final_target = mafia_target;
    }
    if (gs.mafia_last_votes.empty() && mafia_target != -1) {
        gs.mafia_last_votes.push_back(mafia_target);
    }

    auto join_ids = [](const std::vector<int> &values) {
        std::string out;
        for (size_t i = 0; i < values.size(); ++i) {
            if (i) out += ", ";
            out += std::to_string(values[i]);
        }
        return out;
    };

    std::vector<int> mafia_valid_votes;
    mafia_valid_votes.reserve(gs.mafia_last_votes.size());
    for (int v : gs.mafia_last_votes) if (v != -1) mafia_valid_votes.push_back(v);

    if (!mafia_valid_votes.empty()) {
        std::vector<int> unique_votes = mafia_valid_votes;
        std::sort(unique_votes.begin(), unique_votes.end());
        unique_votes.erase(std::unique(unique_votes.begin(), unique_votes.end()), unique_votes.end());
        std::string msg;
        if (unique_votes.size() == 1) {
            msg = "Мафия выбрала игрока " + std::to_string(unique_votes.front()) + ".";
            if (gs.mafia_final_target != -1 && gs.mafia_final_target != unique_votes.front()) {
                msg += " Итоговая цель: " + std::to_string(gs.mafia_final_target) + ".";
            }
        } else {
            msg = "Мафия выбрала игроков " + join_ids(unique_votes) + ".";
            if (gs.mafia_final_target != -1) {
                msg += gs.mafia_choice_random ? " Итоговая цель выбрана случайно: " : " Итоговая цель: ";
                msg += std::to_string(gs.mafia_final_target) + ".";
            }
        }
        log.log_night_action(gs.round, msg);
    } else {
        log.log_night_action(gs.round, "Мафия не выбрала цель.");
    }

    if (maniac_id != -1) {
        if (maniac_target == -1) {
            log.log_night_action(gs.round, "Маньяк пропустил ход.");
        } else {
            log.log_night_action(gs.round, "Маньяк выбрал игрока " + std::to_string(maniac_target) + ".");
        }
    }

    if (commissioner_ptr) {
        if (commissioner_shot_target != -1) {
            log.log_night_action(gs.round, "Комиссар, выстрел: цель " + std::to_string(commissioner_shot_target) + ".");
            if (human_is_commissioner) {
                std::cout << "[Комиссар] Выстрел по цели " << commissioner_shot_target << " — ожидаем результат." << '\n';
            }
        } else if (commissioner_inspect_target != -1) {
            commissioner_ptr->apply_inspection_result(*this);
            auto align = alignment_of(commissioner_inspect_target);
            const std::string inspected_role = role_name_for(commissioner_inspect_target);
            if (align == Alignment::Unknown) {
                log.log_night_action(gs.round, "Комиссар, результат проверки: неизвестно-" + std::to_string(commissioner_inspect_target) + ".");
                if (human_is_commissioner) {
                    std::cout << "[Комиссар] Проверка не дала результата." << '\n';
                }
            } else {
                Alignment report = (align == Alignment::Maniac) ? Alignment::Civilian : align;
                if (inspected_role == "Ниндзя") {
                    report = Alignment::Civilian;
                }
                std::string kind = (report == Alignment::Mafia) ? "мафия" : "мирный";
                std::string line = "Комиссар, результат проверки: " + kind + "-" + std::to_string(commissioner_inspect_target) + ".";
                log.log_night_action(gs.round, line);
                if (human_is_commissioner) {
                    std::cout << "[Комиссар] Результат проверки: " << kind << " (" << commissioner_inspect_target << ")" << '\n';
                }
            }
        } else {
            log.log_night_action(gs.round, "Комиссар действий не предпринял.");
            if (human_is_commissioner) {
                std::cout << "[Комиссар] Действий этой ночью не было." << '\n';
            }
        }
    } else {
        log.log_night_action(gs.round, "Комиссар отсутствует." );
    }

    std::vector<std::string> witness_reports;
    auto register_witness_attempt = [&](const std::string &attacker_desc, int target) {
        if (target == -1) return;
        if (gs.witness_target == -1 || gs.witness_target != target) return;
        std::string msg = "Свидетель видел, что " + attacker_desc + " пытался убить игрока " + std::to_string(target) + ".";
        if (std::find(witness_reports.begin(), witness_reports.end(), msg) == witness_reports.end()) {
            witness_reports.emplace_back(std::move(msg));
        }
    };

    register_witness_attempt("мафия", mafia_target);
    if (maniac_target != -1) {
        std::string attacker = "маньяк";
        if (maniac_id != -1) attacker += " (игрок " + std::to_string(maniac_id) + ")";
        register_witness_attempt(attacker, maniac_target);
    }
    if (commissioner_shot_target != -1) {
        register_witness_attempt("комиссар", commissioner_shot_target);
    }

    auto kill = [&](int vid, Alignment source, const std::string &by) {
        if (vid == -1) return;
        if (vid == doctor_target && doctor_target != -1) {
            if (!doctor_saved_someone) {
                doctor_saved_someone = true;
                doctor_saved_player = vid;
                doctor_saved_by = by;
                announce("Доктор спас игрока " + std::to_string(vid) + " от " + by);
                if (human_is_doctor) {
                    std::cout << "[Доктор] Вам удалось спасти игрока " << vid << "." << '\n';
                }
            }
            return;
        }
        if (source == Alignment::Maniac) {
            auto it_orig = gs.original_roles.find(vid);
            if (it_orig != gs.original_roles.end() && prevents_maniac_kill(it_orig->second)) {
                std::string msg = "Маньяк не смог убить быка (игрок " + std::to_string(vid) + ").";
                log.log_night_action(gs.round, msg);
                announce(msg);
                std::cout << "Маньяк не убивал в эту ночь." << '\n';
                if (maniac_id == gs.human_id && gs.human_enabled && gs.alive.count(gs.human_id)) {
                    std::cout << "[Маньяк] Цель " << vid << " оказалась неуязвимой." << '\n';
                }
                return;
            }
        }
        if (gs.alive.count(vid)) {
            announce("Игрок " + std::to_string(vid) + " погиб от " + by);
            log.log_night_action(gs.round, "Игрок " + std::to_string(vid) + " погиб от " + by + '.');
            auto it_orig = gs.original_roles.find(vid);
            if (it_orig != gs.original_roles.end() && is_doctor(it_orig->second)) {
                doctor_died = true;
            }
            gs.alive.erase(vid);
        }
    };

    kill(mafia_target, Alignment::Mafia, "мафии");
    kill(maniac_target, Alignment::Maniac, "маньяка");
    kill(commissioner_shot_target, Alignment::Civilian, "комиссара");

    if (!doctor_ids.empty()) {
        std::string result = "Доктор, результат лечения: ";
        if (doctor_target == -1) {
            result += "пропуск.";
        } else if (doctor_saved_someone) {
            result += "успех — спас игрока " + std::to_string(doctor_saved_player) + " от " + doctor_saved_by + ".";
        } else if (doctor_died) {
            result += "доктор погиб, лечение не сработало (цель " + std::to_string(doctor_target) + ").";
        } else {
            result += "промах — цель " + std::to_string(doctor_target) + " не подвергалась нападению.";
        }
        log.log_night_action(gs.round, result);
    }

    if (commissioner_ptr && commissioner_shot_target != -1) {
        bool target_alive = gs.alive.count(commissioner_shot_target) != 0;
        std::string outcome = "Комиссар, выстрел: цель " + std::to_string(commissioner_shot_target) + " — " + (target_alive ? "цель выжила." : "цель устранена.");
        log.log_night_action(gs.round, outcome);
        if (human_is_commissioner) {
            std::cout << "[Комиссар] Выстрел по цели " << commissioner_shot_target << (target_alive ? " — цель спасена." : " — цель поражена.") << '\n';
        }
    }

    if (!witness_reports.empty()) {
        std::string combined;
        for (size_t i = 0; i < witness_reports.size(); ++i) {
            if (i) combined += '\n';
            combined += witness_reports[i];
            log.log_night_action(gs.round, witness_reports[i]);
            announce(witness_reports[i]);
        }
        gs.witness_message = combined;
    }
}


bool Host::game_over() {
    int m = gs.mafia_count();
    int civ = gs.civilians_count();
    int man = gs.maniac_count();

    if (m == 0 && man == 0) {
        announce("Победили мирные жители!");
        log.log_final("Победа: мирные жители");
        return true;
    }
    if (m >= civ) {
        announce("Победила мафия!");
        log.log_final("Победа: мафия");
        return true;
    }
    if (man == 1 && civ == 1 && m == 0) {
        announce("Маньяк победил!");
        log.log_final("Победа: маньяк");
        return true;
    }
    return false;
}

Task<> Host::run() {
    for (gs.round = 1;; ++gs.round) {
                std::vector<int> ids;
        ids.reserve(gs.alive.size());
        for (auto &[id, _] : gs.alive) ids.push_back(id);
        log.start_day(gs.round, ids);

        // === День: каждый игрок голосует ===
        gs.day_votes.clear();
        {
                for (int id : gs.alive | std::views::keys) {
                    auto &p = gs.alive.at(id);
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
        gs.witness_target = -1;
        gs.witness_observer = -1;
        gs.witness_message.reset();
        gs.mafia_last_votes.clear();
        gs.mafia_final_target = -1;
        gs.mafia_choice_random = false;
        {
            bool human_is_mafia = false;
            if (gs.human_enabled && gs.alive.count(gs.human_id)) {
                human_is_mafia = (alignment_of(gs.human_id) == Alignment::Mafia);
            }

            for (int id : gs.alive | std::views::keys) {
                auto &p = gs.alive.at(id);
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
                        gs.mafia_last_votes = chosen;
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
                            gs.mafia_choice_random = (unique_max > 1);
                        }

                        if (final_target != -1) {
                            for (int id : mafia_ids) gs.alive.at(id)->set_target(final_target);
                            gs.mafia_final_target = final_target;
                        }
                    }
                }
                if (gs.mafia_last_votes.empty()) {
                    for (int id : mafia_ids) {
                        auto it = gs.alive.find(id);
                        if (it == gs.alive.end()) continue;
                        int t = it->second->get_target();
                        if (t != -1) gs.mafia_last_votes.push_back(t);
                    }
                }
                if (gs.mafia_final_target == -1 && !mafia_ids.empty()) {
                    for (int id : mafia_ids) {
                        auto it = gs.alive.find(id);
                        if (it == gs.alive.end()) continue;
                        int t = it->second->get_target();
                        if (t != -1) {
                            gs.mafia_final_target = t;
                            break;
                        }
                    }
                }
            }

            for (int id : gs.alive | std::views::keys) {
                auto &p = gs.alive.at(id);
                if (alignment_of(id) == Alignment::Mafia) continue;
                if (gs.human_enabled && id == gs.human_id) continue;
                co_await p->act(gs, *this, id, log, gs.round, /*mafiaPhase=*/false);
            }
        }
        resolve_night();
        if (game_over()) co_return;
    }
}
