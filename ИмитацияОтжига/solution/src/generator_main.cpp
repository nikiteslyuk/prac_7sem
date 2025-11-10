#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

struct RangeSpec {
    unsigned start{};
    unsigned step{};
    unsigned end{};
};

RangeSpec parse_range(const std::string& text) {
    RangeSpec spec{};
    size_t first = text.find(':');
    size_t second = text.rfind(':');
    if (first == std::string::npos || second == first) {
        throw std::invalid_argument("Range must be in start:step:end format");
    }
    spec.start = static_cast<unsigned>(std::stoul(text.substr(0, first)));
    spec.step = static_cast<unsigned>(std::stoul(text.substr(first + 1, second - first - 1)));
    spec.end = static_cast<unsigned>(std::stoul(text.substr(second + 1)));
    if (spec.step == 0 || spec.start == 0 || spec.end < spec.start) {
        throw std::invalid_argument("Invalid range values");
    }
    return spec;
}

struct GeneratorOptions {
    std::filesystem::path output_dir = "data";
    RangeSpec processors{2, 2, 20};
    RangeSpec tasks{100, 100, 2000};
    unsigned min_duration = 5;
    unsigned max_duration = 20;
    uint64_t seed = static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
};

GeneratorOptions parse_args(int argc, char** argv) {
    GeneratorOptions opts;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto require_value = [&](const std::string& name) -> std::string {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for " + name);
            }
            return argv[++i];
        };
        if (arg == "--output-dir") {
            opts.output_dir = require_value(arg);
        } else if (arg == "--procs") {
            opts.processors = parse_range(require_value(arg));
        } else if (arg == "--tasks") {
            opts.tasks = parse_range(require_value(arg));
        } else if (arg == "--min-duration") {
            opts.min_duration = static_cast<unsigned>(std::stoul(require_value(arg)));
        } else if (arg == "--max-duration") {
            opts.max_duration = static_cast<unsigned>(std::stoul(require_value(arg)));
        } else if (arg == "--seed") {
            opts.seed = std::stoull(require_value(arg));
        } else {
            throw std::invalid_argument("Unknown argument: " + arg);
        }
    }

    if (opts.min_duration > opts.max_duration) {
        throw std::invalid_argument("min-duration must be <= max-duration");
    }
    return opts;
}

std::vector<unsigned> collect_values(const RangeSpec& spec) {
    std::vector<unsigned> values;
    for (unsigned value = spec.start; value <= spec.end; value += spec.step) {
        values.push_back(value);
    }
    return values;
}

int main(int argc, char** argv) {
    try {
        auto options = parse_args(argc, argv);
        std::filesystem::create_directories(options.output_dir);
        std::mt19937 rng(options.seed);
        std::uniform_int_distribution<unsigned> dist(options.min_duration, options.max_duration);

        auto proc_values = collect_values(options.processors);
        auto task_values = collect_values(options.tasks);

        for (auto proc : proc_values) {
            for (auto tasks : task_values) {
                std::filesystem::path file_path = options.output_dir / ("dataset_P" + std::to_string(proc) + "_T" + std::to_string(tasks) + ".csv");
                std::ofstream out(file_path);
                if (!out.is_open()) {
                    std::cerr << "Failed to write " << file_path << "\n";
                    return 1;
                }
                out << proc << ' ' << tasks << '\n';
                for (unsigned job = 0; job < tasks; ++job) {
                    out << dist(rng) << '\n';
                }
            }
        }
        std::cout << "Generated " << proc_values.size() * task_values.size()
                  << " datasets into " << options.output_dir << "\n";
    } catch (const std::exception& ex) {
        std::cerr << "Generator error: " << ex.what() << "\n";
        return 1;
    }
    return 0;
}
