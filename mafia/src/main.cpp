#include <iostream>
#include <vector>
#include <string>
#include <limits>
#include <algorithm>
#include <cctype>
#include <yaml-cpp/yaml.h>

#include "smart_ptr.hpp"
#include "logger.hpp"
#include "roles.hpp"
#include "host.hpp"
#include "human.hpp"
#include "game_state.hpp"

// Разрешённые роли в конфиге: Mafia, Bull, Commissioner, Doctor, Civilian, Witness, Ninja, Maniac
static smart_ptr<PlayerBase> make_role3(const std::string& role_name) {
    if (role_name == "Mafia")        return make_role<Mafia>();
    if (role_name == "Bull")         return make_role<Bull>();
    if (role_name == "Commissioner") return make_role<Commissioner>();
    if (role_name == "Doctor")       return make_role<Doctor>();
    if (role_name == "Civilian")     return make_role<Civilian>();
    if (role_name == "Witness")      return make_role<Witness>();
    if (role_name == "Ninja")        return make_role<Ninja>();
    if (role_name == "Maniac")       return make_role<Maniac>();
    return smart_ptr<PlayerBase>(); // неизвестная/запрещённая роль
}

int main(int argc, char **argv) {
    YAML::Node cfg;
    std::string config_path = "config.yaml";
    if (argc > 1 && argv[1] && *argv[1]) {
        config_path = argv[1];
    }
    try {
        cfg = YAML::LoadFile(config_path);
    } catch (const std::exception &e) {
        std::cerr << "Не удалось загрузить конфиг '" << config_path << "': " << e.what() << "\n";
        return 1;
    }

    int N = cfg["players"] ? cfg["players"].as<int>() : 10;
    int k = cfg["k"] ? cfg["k"].as<int>() : 4;
    bool human = cfg["human"] ? cfg["human"].as<bool>() : false;
    bool announce_open = cfg["announce"] ? (cfg["announce"].as<std::string>()=="open") : true;
    bool debug_mode = cfg["debug"] ? cfg["debug"].as<bool>() : false;
    std::string debug_role_cfg = cfg["debug_role"] ? cfg["debug_role"].as<std::string>() : std::string{};

    if (debug_mode) {
        human = true;
    }

    GameState gs;
    gs.k = k;
    gs.announce_open = announce_open;
    gs.human_enabled = human;

    int next_id = 1;

    // 1) Явный список ролей (обратная совместимость)
    if (cfg["roles"] && cfg["roles"].IsSequence()) {
        for (const auto& node : cfg["roles"]) {
            std::string r = node.as<std::string>();
            auto p = make_role3(r);
            if (!p.get()) {
                std::cerr << "WARN: роль \"" << r << "\" не поддерживается (Don запрещён). Пропуск.\n";
                continue;
            }
            gs.alive[next_id] = std::move(p);
            ++next_id;
        }
        while ((int)gs.alive.size() < N) {
            gs.alive[next_id] = smart_ptr<PlayerBase>(new Civilian());
            ++next_id;
        }
    } else if (cfg["mafia"] || cfg["commissioner"] || cfg["doctor"] || cfg["civilians"]) {
        // 2) Формат с количествами ролей
        int mafia_count = cfg["mafia"] ? std::max(0, cfg["mafia"].as<int>()) : 0;
        int comm_count  = cfg["commissioner"] ? std::max(0, cfg["commissioner"].as<int>()) : 0;
        int doc_count   = cfg["doctor"] ? std::max(0, cfg["doctor"].as<int>()) : 0;
        int civ_count   = cfg["civilians"] ? std::max(0, cfg["civilians"].as<int>()) : 0;
        int witness_count = cfg["witness"] ? std::max(0, cfg["witness"].as<int>()) : 0;
        int bull_count    = cfg["bull"] ? std::max(0, cfg["bull"].as<int>()) : 0;
        int ninja_count   = cfg["ninja"] ? std::max(0, cfg["ninja"].as<int>()) : 0;
        int maniac_count  = cfg["maniac"] ? std::max(0, cfg["maniac"].as<int>()) : 0;

        int sum_counts = mafia_count + comm_count + doc_count + civ_count + witness_count + bull_count + ninja_count + maniac_count;
        if (sum_counts > 0) {
            // если players не задан, возьмём сумму
            if (!cfg["players"]) N = sum_counts;
        }

        std::vector<smart_ptr<PlayerBase>> roles_pool;
        roles_pool.reserve(N);
        for (int i = 0; i < mafia_count; ++i)        roles_pool.emplace_back(new Mafia());
        for (int i = 0; i < comm_count;  ++i)        roles_pool.emplace_back(new Commissioner());
        for (int i = 0; i < doc_count;   ++i)        roles_pool.emplace_back(new Doctor());
        for (int i = 0; i < witness_count; ++i)      roles_pool.emplace_back(new Witness());
        for (int i = 0; i < bull_count; ++i)         roles_pool.emplace_back(new Bull());
        for (int i = 0; i < ninja_count; ++i)        roles_pool.emplace_back(new Ninja());
        for (int i = 0; i < maniac_count; ++i)       roles_pool.emplace_back(new Maniac());
        for (int i = 0; i < civ_count;   ++i)        roles_pool.emplace_back(new Civilian());
        while ((int)roles_pool.size() < N) roles_pool.emplace_back(new Civilian());

        std::vector<int> ids;
        ids.reserve(N);
        for (int i = 1; i <= N; ++i) ids.push_back(i);
        std::shuffle(ids.begin(), ids.end(), gs.rng);

        for (int i = 0; i < N; ++i) {
            gs.alive[ids[i]] = std::move(roles_pool[i]);
        }
    } else {
        int mafia_count = std::max(1, N / k);

        std::vector<smart_ptr<PlayerBase>> roles_pool;
        roles_pool.reserve(N);
        for (int i = 0; i < mafia_count; ++i) roles_pool.emplace_back(new Mafia());
        roles_pool.emplace_back(new Commissioner());
        roles_pool.emplace_back(new Doctor());
        while ((int)roles_pool.size() < N) roles_pool.emplace_back(new Civilian());

        std::vector<int> ids;
        ids.reserve(N);
        for (int i = 1; i <= N; ++i) ids.push_back(i);
        std::shuffle(ids.begin(), ids.end(), gs.rng);

        for (int i = 0; i < N; ++i) {
            gs.alive[ids[i]] = std::move(roles_pool[i]);
        }
    }

    if (debug_mode) {
        auto to_lower = [](std::string s) {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
            return s;
        };

        std::string lowered = to_lower(debug_role_cfg);
        std::string canonical;
        std::string desired_role_ru;
        if (lowered == "mafia" || lowered == "maf" || lowered == "мафия") {
            canonical = "Mafia";
            desired_role_ru = "Мафия";
        } else if (lowered == "commissioner" || lowered == "comm" || lowered == "комиссар") {
            canonical = "Commissioner";
            desired_role_ru = "Комиссар";
        } else if (lowered == "doctor" || lowered == "doc" || lowered == "доктор") {
            canonical = "Doctor";
            desired_role_ru = "Доктор";
        } else if (lowered == "civilian" || lowered == "civ" || lowered == "мирный") {
            canonical = "Civilian";
            desired_role_ru = "Мирный";
        } else if (lowered == "witness" || lowered == "w" || lowered == "свидетель") {
            canonical = "Witness";
            desired_role_ru = "Свидетель";
        } else if (lowered == "bull" || lowered == "бык") {
            canonical = "Bull";
            desired_role_ru = "Бык";
        } else if (lowered == "ninja" || lowered == "нинзя" || lowered == "ниндзя") {
            canonical = "Ninja";
            desired_role_ru = "Ниндзя";
        } else if (lowered == "maniac" || lowered == "maniak" || lowered == "маньяк") {
            canonical = "Maniac";
            desired_role_ru = "Маньяк";
        }

        if (!canonical.empty()) {
            auto matches_role = [&](PlayerBase *player) {
                if (!player) return false;
                if (desired_role_ru == "Мафия")     return dynamic_cast<Mafia *>(player) != nullptr;
                if (desired_role_ru == "Комиссар")  return dynamic_cast<Commissioner *>(player) != nullptr;
                if (desired_role_ru == "Доктор")     return dynamic_cast<Doctor *>(player) != nullptr;
                if (desired_role_ru == "Мирный")     return dynamic_cast<Civilian *>(player) != nullptr;
                if (desired_role_ru == "Свидетель")  return dynamic_cast<Witness *>(player) != nullptr;
                if (desired_role_ru == "Бык")        return dynamic_cast<Bull *>(player) != nullptr;
                if (desired_role_ru == "Ниндзя")     return dynamic_cast<Ninja *>(player) != nullptr;
                if (desired_role_ru == "Маньяк")     return dynamic_cast<Maniac *>(player) != nullptr;
                return false;
            };

            int chosen_id = -1;
            for (auto &[id, player] : gs.alive) {
                if (matches_role(player.get())) {
                    chosen_id = id;
                    break;
                }
            }

            if (chosen_id == -1 && !gs.alive.empty()) {
                auto new_role = make_role3(canonical);
                if (new_role.get()) {
                    chosen_id = gs.alive.begin()->first;
                    gs.alive[chosen_id] = std::move(new_role);
                }
            }

            if (chosen_id != -1) {
                gs.human_id = chosen_id;
            } else {
                std::cerr << "WARN: не удалось назначить роль для режима debug. Будет выбран случайный игрок.\n";
            }
        } else if (!debug_role_cfg.empty()) {
            std::cerr << "WARN: debug_role '" << debug_role_cfg << "' не поддерживается. Будет выбран случайный игрок.\n";
        }
    }

    if (human) {
        if (!(debug_mode && gs.human_id != -1 && gs.alive.contains(gs.human_id))) {
            std::vector<int> ids; ids.reserve(gs.alive.size());
            for (auto& [id, _] : gs.alive) ids.push_back(id);
            gs.human_id = gs.pick_random(ids);
        }
    }

    Logger log;
    Host host(gs, log);

    if (human) {
        std::string my_role = host.role_name_for(gs.human_id);
        if (my_role.empty()) my_role = "?";
        std::cout << "Вы играете за id: " << gs.human_id
                  << " (" << my_role << ")\n";
        std::cout << "\nКак играть (быстрый гайд):\n"
                  << "- Днём: введите id игрока, за которого голосуете, или -1 чтобы воздержаться.\n"
                  << "- Ночью: у некоторых ролей есть действия (комиссар проверяет, доктор лечит, мафия выбирает цель).\n"
                  << "- Подсказки перед вводом будут в квадратных скобках: [День], [Ночь/Комиссар] и т.п.\n\n";
        if (debug_mode) {
            std::cout << "[debug] Режим отладки активен: роль выбрана из config.yaml\n";
        }
    }

    log.log_game_set(gs.original_roles);
    auto h = host.run();
    h.get();

    auto dump_role = [&](const std::string &role_ru, const std::string &label) {
        std::vector<int> ids;
        for (auto &[id, role] : gs.original_roles) if (role == role_ru) ids.push_back(id);
        if (ids.empty()) return;
        std::cout << label << ": ";
        for (size_t i = 0; i < ids.size(); ++i) {
            if (i) std::cout << ", ";
            std::cout << ids[i];
        }
        std::cout << "\n";
        std::string line = label + ": ";
        for (size_t i = 0; i < ids.size(); ++i) {
            if (i) line += ",";
            line += std::to_string(ids[i]);
        }
        log.log_final(line);
    };

    dump_role("Мафия", "Мафия");
    dump_role("Бык", "Бык");
    dump_role("Ниндзя", "Ниндзя");
    dump_role("Маньяк", "Маньяк");

    log.log_final("Игра завершена");
    std::cout << "Игра завершена\n";
    return 0;
}
