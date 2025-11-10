#pragma once

#include <cmath>
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>
#include <stdexcept>

class RandomProvider {
public:
    static void seed(uint64_t value);
    static uint64_t uniform(uint64_t min_inclusive, uint64_t max_inclusive);
private:
    static thread_local std::mt19937_64 engine_;
};

class ISolution {
public:
    virtual ~ISolution() = default;
    virtual double score() const = 0;
    virtual void mutate() = 0;
    virtual std::unique_ptr<ISolution> clone() const = 0;
};

class ScheduleSolution : public ISolution {
public:
    ScheduleSolution(unsigned proc_num, const std::vector<unsigned>& works);
    ScheduleSolution(const ScheduleSolution& other) = default;
    ScheduleSolution& operator=(const ScheduleSolution& other) = default;
    double score() const override;
    void mutate() override;
    std::unique_ptr<ISolution> clone() const override;
    unsigned processor_count() const { return proc_num_; }
    const std::vector<unsigned>& durations() const { return works_len_; }
private:
    unsigned proc_num_;
    std::vector<unsigned> works_len_;
    std::vector<unsigned> assignment_;
};

class IMutator {
public:
    virtual ~IMutator() = default;
    virtual void mutate(ISolution* sol) const = 0;
};

class Mutator : public IMutator {
public:
    explicit Mutator(unsigned max_mutations);
    void mutate(ISolution* sol) const override;
private:
    unsigned max_mutations_;
};

class ITemperatureDecrease {
public:
    virtual ~ITemperatureDecrease() = default;
    virtual double decrease(double current_temp, unsigned iter_num) const = 0;
};

class BoltzmannTemperatureDecrease : public ITemperatureDecrease {
public:
    double decrease(double current_temp, unsigned iter_num) const override;
};

class CauchyTemperatureDecrease : public ITemperatureDecrease {
public:
    double decrease(double current_temp, unsigned iter_num) const override;
};

class LogTemperatureDecrease : public ITemperatureDecrease {
public:
    double decrease(double current_temp, unsigned iter_num) const override;
};

struct AnnealingParams {
    double start_temp;
    unsigned batch_size;
    unsigned patience;
};

class MainCycle {
public:
    MainCycle(const ISolution& start_solution,
              const IMutator& mutator,
              const ITemperatureDecrease& temp_decrease,
              const AnnealingParams& params);
    std::unique_ptr<ISolution> process() const;
private:
    const ISolution& start_solution_;
    const IMutator& mutator_;
    const ITemperatureDecrease& temp_decrease_;
    AnnealingParams params_;
};
