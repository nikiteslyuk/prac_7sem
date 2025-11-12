#include "sa_classes.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <limits>
#include <sstream>
#include <string>
#include <vector>
#include <cstdint>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

bool send_all(int fd, const void* data, size_t bytes) {
    const char* ptr = static_cast<const char*>(data);
    size_t sent = 0;
    while (sent < bytes) {
        ssize_t res = ::send(fd, ptr + sent, bytes - sent, 0);
        if (res <= 0) {
            return false;
        }
        sent += static_cast<size_t>(res);
    }
    return true;
}

bool recv_all(int fd, void* data, size_t bytes) {
    char* ptr = static_cast<char*>(data);
    size_t received = 0;
    while (received < bytes) {
        ssize_t res = ::recv(fd, ptr + received, bytes - received, 0);
        if (res <= 0) {
            return false;
        }
        received += static_cast<size_t>(res);
    }
    return true;
}

struct InstanceData {
    unsigned processors{};
    std::vector<unsigned> tasks;
    std::string label;
};

struct SolverOptions { // параметры запуска основного solver'a
    std::string input_path;
    std::string output_csv;
    std::string label;
    unsigned workers = 1;
    unsigned max_mutations = 20;
    unsigned batch = 5000;
    unsigned patience = 100;
    double start_temp = 1e12;
    uint64_t seed = static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    unsigned runs = 1;
};

SolverOptions parse_args(int argc, char** argv) { // разбираем CLI флаги
    SolverOptions opts;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto require_value = [&](const std::string& name) -> std::string {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for " + name);
            }
            return argv[++i];
        };
        if (arg == "--input") {
            opts.input_path = require_value(arg);
        } else if (arg == "--output") {
            opts.output_csv = require_value(arg);
        } else if (arg == "--label") {
            opts.label = require_value(arg);
        } else if (arg == "--workers") {
            opts.workers = static_cast<unsigned>(std::stoul(require_value(arg)));
        } else if (arg == "--max-mutations") {
            opts.max_mutations = static_cast<unsigned>(std::stoul(require_value(arg)));
        } else if (arg == "--batch") {
            opts.batch = static_cast<unsigned>(std::stoul(require_value(arg)));
        } else if (arg == "--patience") {
            opts.patience = static_cast<unsigned>(std::stoul(require_value(arg)));
        } else if (arg == "--start-temp") {
            opts.start_temp = std::stod(require_value(arg));
        } else if (arg == "--seed") {
            opts.seed = std::stoull(require_value(arg));
        } else if (arg == "--runs") {
            opts.runs = static_cast<unsigned>(std::stoul(require_value(arg)));
        } else {
            throw std::invalid_argument("Unknown argument: " + arg);
        }
    }
    if (opts.input_path.empty()) {
        throw std::invalid_argument("--input is required");
    }
    if (opts.workers == 0) {
        throw std::invalid_argument("--workers must be >= 1");
    }
    if (opts.runs == 0) {
        throw std::invalid_argument("--runs must be >= 1");
    }
    if (opts.label.empty()) {
        opts.label = std::filesystem::path(opts.input_path).stem().string();
    }
    return opts;
}

InstanceData read_instance(const std::string& path, const std::string& label_override) { // читаем CSV датасет
    std::ifstream in(path);
    if (!in.is_open()) {
        throw std::runtime_error("Failed to open input file: " + path);
    }
    unsigned processors = 0;
    unsigned tasks = 0;
    in >> processors >> tasks;
    std::vector<unsigned> lengths;
    lengths.reserve(tasks);
    for (unsigned i = 0; i < tasks; ++i) {
        unsigned value = 0;
        in >> value;
        lengths.push_back(value);
    }
    if (lengths.size() != tasks) {
        throw std::runtime_error("Input file ended prematurely");
    }
    InstanceData data;
    data.processors = processors;
    data.tasks = std::move(lengths);
    data.label = label_override;
    return data;
}

std::string make_socket_path() { // создаём уникальный путь для UNIX-сокета
    auto base = std::filesystem::temp_directory_path();
    auto pid = static_cast<long>(::getpid());
    auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    std::ostringstream oss;
    oss << "sa_socket_" << pid << "_" << timestamp;
    return (base / oss.str()).string();
}

void write_results_row(const SolverOptions& opts, // дописываем строку в CSV отчёта
                       const InstanceData& data,
                       uint64_t best_score,
                       double avg_score,
                       double elapsed_seconds) {
    if (opts.output_csv.empty()) {
        return;
    }
    bool write_header = !std::filesystem::exists(opts.output_csv);
    std::ofstream out(opts.output_csv, std::ios::app);
    if (!out.is_open()) {
        throw std::runtime_error("Unable to write results CSV: " + opts.output_csv);
    }
    if (write_header) {
        out << "label,processors,tasks,workers,runs,seed,best_score,avg_score,elapsed_seconds\n";
    }
    out << opts.label << ','
        << data.processors << ','
        << data.tasks.size() << ','
        << opts.workers << ','
        << opts.runs << ','
        << opts.seed << ','
        << best_score << ','
        << avg_score << ','
        << elapsed_seconds << '\n';
}

void worker_process(const std::string& socket_path, // тело дочернего процесса
                    const ScheduleSolution& start_solution,
                    const Mutator& mutator,
                    const ITemperatureDecrease& cooling,
                    const AnnealingParams& params,
                    uint64_t worker_seed) {
    RandomProvider::seed(worker_seed);
    int client_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (client_fd < 0) {
        std::perror("socket");
        _exit(1);
    }
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socket_path.c_str());
    if (::connect(client_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::perror("connect");
        _exit(1);
    }
    auto result = MainCycle(start_solution, mutator, cooling, params).process();
    uint64_t score = static_cast<uint64_t>(result->score());
    auto concrete = dynamic_cast<ScheduleSolution*>(result.get());
    if (!concrete) {
        std::perror("cast");
        _exit(1);
    }
    const auto& assignment = concrete->assignment();
    uint32_t size = static_cast<uint32_t>(assignment.size());
    if (!send_all(client_fd, &score, sizeof(score)) ||
        !send_all(client_fd, &size, sizeof(size))) { // отправляем родителю score и размер расписания
        std::perror("send");
        _exit(1);
    }
    if (size > 0) {
        std::vector<uint32_t> buffer(size);
        for (size_t i = 0; i < size; ++i) {
            buffer[i] = assignment[i];
        }
        if (!send_all(client_fd, buffer.data(), buffer.size() * sizeof(uint32_t))) { // передаём само лучшее назначение
            std::perror("send");
            _exit(1);
        }
    }
    ::close(client_fd);
    _exit(0);
}

int main(int argc, char** argv) {
    try {
        auto options = parse_args(argc, argv); // читаем параметры запуска
        auto instance = read_instance(options.input_path, options.label); // подгружаем датасет
        ScheduleSolution start_solution(instance.processors, instance.tasks);
        Mutator mutator(options.max_mutations);
        BoltzmannTemperatureDecrease cooling;
        AnnealingParams params{options.start_temp, options.batch, options.patience};

        double total_time = 0.0;
        double score_sum = 0.0;
        uint64_t global_best = std::numeric_limits<uint64_t>::max();
        std::vector<unsigned> best_assignment_overall = start_solution.assignment();
        unsigned runs_without_improvement = 0;
        unsigned executed_runs = 0;

        for (unsigned run = 0; run < options.runs; ++run) {
            ++executed_runs;
            auto socket_path = make_socket_path();
            int server_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
            if (server_fd < 0) {
                throw std::runtime_error("socket() failed");
            }
            sockaddr_un addr{};
            addr.sun_family = AF_UNIX;
            std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socket_path.c_str());
            ::unlink(socket_path.c_str());
            if (::bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
                throw std::runtime_error("bind() failed");
            }
            if (::listen(server_fd, options.workers) < 0) {
                throw std::runtime_error("listen() failed");
            }

            auto start_time = std::chrono::steady_clock::now();

            for (unsigned worker_id = 0; worker_id < options.workers; ++worker_id) { // форкаем процессы-воркеры и передаём им обновлённое глобальное решение через start_solution
                pid_t pid = ::fork();
                if (pid == 0) {
                    uint64_t worker_seed = options.seed + run * 7919 + worker_id * 9973 + static_cast<uint64_t>(::getpid());
                    worker_process(socket_path, start_solution, mutator, cooling, params, worker_seed);
                } else if (pid < 0) {
                    throw std::runtime_error("fork() failed");
                }
            }

            uint64_t sum_scores = 0;
            uint64_t best_score = std::numeric_limits<uint64_t>::max();
            std::vector<unsigned> best_assignment_run;
            bool has_best_assignment = false;

            for (unsigned i = 0; i < options.workers; ++i) { // собираем лучшие решения и синхронизируемся через сокет
                int client_fd = ::accept(server_fd, nullptr, nullptr);
                if (client_fd < 0) {
                    throw std::runtime_error("accept() failed");
                }
                uint64_t score = 0;
                uint32_t assignment_size = 0;
                if (!recv_all(client_fd, &score, sizeof(score)) ||
                    !recv_all(client_fd, &assignment_size, sizeof(assignment_size))) {
                    throw std::runtime_error("Failed to receive worker result");
                }
                std::vector<unsigned> assignment_data;
                if (assignment_size > 0) {
                    std::vector<uint32_t> buffer(assignment_size);
                    if (!recv_all(client_fd, buffer.data(), buffer.size() * sizeof(uint32_t))) {
                        throw std::runtime_error("Failed to receive assignment");
                    }
                    assignment_data.reserve(assignment_size);
                    for (auto value : buffer) {
                        assignment_data.push_back(static_cast<unsigned>(value));
                    }
                }
                sum_scores += score;
                if (score < best_score) {
                    best_score = score;
                    best_assignment_run = std::move(assignment_data);
                    has_best_assignment = true;
                }
                ::close(client_fd);
            }

            for (unsigned i = 0; i < options.workers; ++i) {
                ::wait(nullptr);
            }

            auto end_time = std::chrono::steady_clock::now();
            double elapsed_seconds = std::chrono::duration<double>(end_time - start_time).count();
            total_time += elapsed_seconds;
            score_sum += static_cast<double>(best_score);
            bool improved = false;
            if (has_best_assignment && best_score < global_best) {
                global_best = best_score;
                best_assignment_overall = best_assignment_run;
                start_solution.set_assignment(best_assignment_overall);
                improved = true;
            }
            if (improved) {
                runs_without_improvement = 0;
            } else {
                ++runs_without_improvement;
            }
            std::cout << "Global best after run " << (run + 1) << ": " << global_best << std::endl;
            if (runs_without_improvement >= 10) { // критерий останова
                std::cout << "No improvement for 10 runs, stopping." << std::endl;
                ::close(server_fd);
                ::unlink(socket_path.c_str());
                break;
            }

            ::close(server_fd);
            ::unlink(socket_path.c_str());
        }

        double avg_time = total_time / std::max(1u, executed_runs);
        double avg_score = score_sum / std::max(1u, executed_runs);

        std::cout << "Dataset: " << options.label << "\n"
                  << "Workers: " << options.workers << "\n"
                  << "Runs: " << std::max(1u, executed_runs) << "\n"
                  << "Best score: " << global_best << "\n"
                  << "Average score: " << avg_score << "\n"
                  << "Average time (s): " << avg_time << "\n";

        SolverOptions final_options = options;
        final_options.runs = std::max(1u, executed_runs);
        write_results_row(final_options, instance, global_best, avg_score, avg_time);
    } catch (const std::exception& ex) {
        std::cerr << "Solver error: " << ex.what() << "\n";
        return 1;
    }
    return 0;
}
