#include "sa_classes.h"

#include <algorithm>
#include <cassert>

thread_local std::mt19937_64 RandomProvider::engine_{std::random_device{}()};

void RandomProvider::seed(uint64_t value) {
    engine_.seed(value);
}

uint64_t RandomProvider::uniform(uint64_t min_inclusive, uint64_t max_inclusive) {
    std::uniform_int_distribution<uint64_t> dist(min_inclusive, max_inclusive);
    return dist(engine_);
}

ScheduleSolution::ScheduleSolution(unsigned proc_num, const std::vector<unsigned>& works)
    : proc_num_(proc_num), works_len_(works), assignment_(works.size(), 0) {
    if (proc_num_ == 0) {
        throw std::invalid_argument("Processor count must be positive");
    }
    for (size_t i = 0; i < assignment_.size(); ++i) {
        assignment_[i] = static_cast<unsigned>(i % proc_num_);
    }
}

double ScheduleSolution::score() const {
    // Группируем задачи по процессорам, чтобы стоимость расчёта не зависела от того,
    // как часто один и тот же процессор встречается в исходном порядке задач.
    std::vector<std::vector<unsigned>> by_proc(proc_num_);
    for (size_t task = 0; task < works_len_.size(); ++task) {
        by_proc[assignment_[task]].push_back(works_len_[task]);
    }
    unsigned long long total_completion = 0;
    for (auto& queue : by_proc) {
        unsigned long long proc_time = 0;
        for (auto duration : queue) {
            proc_time += duration;
            total_completion += proc_time;
        }
    }
    return static_cast<double>(total_completion);
}

void ScheduleSolution::mutate() {
    if (works_len_.empty() || proc_num_ <= 1) {
        return;
    }
    size_t work_idx = RandomProvider::uniform(0, works_len_.size() - 1);
    unsigned current_proc = assignment_[work_idx];
    // pick a different processor without unbounded retries
    unsigned new_proc = static_cast<unsigned>(RandomProvider::uniform(0, proc_num_ - 2));
    if (new_proc >= current_proc) {
        new_proc += 1;
    }
    assignment_[work_idx] = new_proc;
}

std::unique_ptr<ISolution> ScheduleSolution::clone() const {
    return std::make_unique<ScheduleSolution>(*this);
}

void ScheduleSolution::set_assignment(const std::vector<unsigned>& assignment) {
    if (assignment.size() != assignment_.size()) {
        throw std::invalid_argument("assignment size mismatch");
    }
    assignment_ = assignment;
}

Mutator::Mutator(unsigned max_mutations) : max_mutations_(max_mutations) {
    if (max_mutations_ == 0) {
        throw std::invalid_argument("max_mutations must be > 0");
    }
}

void Mutator::mutate(ISolution* sol) const {
    unsigned repeats = static_cast<unsigned>(RandomProvider::uniform(1, max_mutations_));
    while (repeats--) {
        sol->mutate();
    }
}

double BoltzmannTemperatureDecrease::decrease(double initial_temp, unsigned iter_num) const {
    if (iter_num == 0) {
        return initial_temp;
    }
    double denom = std::log(2.0 + static_cast<double>(iter_num));
    if (denom <= 0.0) {
        return initial_temp;
    }
    return initial_temp / denom;
}

double CauchyTemperatureDecrease::decrease(double initial_temp, unsigned iter_num) const {
    if (iter_num == 0) {
        return initial_temp;
    }
    double denom = 1.0 + static_cast<double>(iter_num);
    return initial_temp / denom;
}

double LogTemperatureDecrease::decrease(double initial_temp, unsigned iter_num) const {
    if (iter_num == 0) {
        return initial_temp;
    }
    double shifted_iter = static_cast<double>(iter_num) + 2.0;
    double numerator = std::log(shifted_iter);
    double denom = shifted_iter;
    if (denom <= 0.0 || numerator <= 0.0) {
        return initial_temp;
    }
    return initial_temp * (numerator / denom);
}

MainCycle::MainCycle(const ISolution& start_solution,
                     const IMutator& mutator,
                     const ITemperatureDecrease& temp_decrease,
                     const AnnealingParams& params)
    : start_solution_(start_solution),
      mutator_(mutator),
      temp_decrease_(temp_decrease),
      params_(params) {
    if (params_.batch_size == 0 || params_.patience == 0) {
        throw std::invalid_argument("batch_size and patience must be positive");
    }
}

std::unique_ptr<ISolution> MainCycle::process() const {
    auto current = start_solution_.clone();
    auto best = start_solution_.clone();
    double current_score = current->score();
    double best_score = current_score;
    unsigned iterations_without_improvement = 0;
    unsigned long long iteration = 0;

    while (iterations_without_improvement < params_.patience) {
        bool improved = false;
        for (unsigned i = 0; i < params_.batch_size; ++i) {
            double temperature = temp_decrease_.decrease(params_.start_temp, iteration);
            if (temperature < 1e-9) {
                temperature = 1e-9;
            }
            ++iteration;
            auto candidate = current->clone();
            mutator_.mutate(candidate.get());
            double candidate_score = candidate->score();

            if (candidate_score < best_score) {
                best_score = candidate_score;
                best = candidate->clone();
                improved = true;
            }

            if (candidate_score < current_score) {
                current_score = candidate_score;
                current = std::move(candidate);
            } else {
                double prob = std::exp((current_score - candidate_score) / temperature);
                double threshold = static_cast<double>(RandomProvider::uniform(0, 10'000'000)) / 10'000'000.0;
                if (prob >= threshold) {
                    current_score = candidate_score;
                    current = std::move(candidate);
                }
            }
        }

        if (improved) {
            iterations_without_improvement = 0;
        } else {
            iterations_without_improvement += 1;
        }
    }

    return best;
}
