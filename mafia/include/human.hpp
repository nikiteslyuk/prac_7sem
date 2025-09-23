#pragma once

#include <string>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <future>
#include <thread>
#include <chrono>

#include "task.hpp"
#include "game_state.hpp"
#include "logger.hpp"
#include "host.hpp"

// awaiter для std::future<T>
template<typename T>
struct FutureAwaiter {
    std::future<T> fut;

    bool await_ready() const {
        using namespace std::chrono_literals;
        return fut.wait_for(0s) == std::future_status::ready;
    }

    void await_suspend(std::coroutine_handle<> h) {
        std::thread([h, f = std::move(fut)]() mutable {
            f.wait();
            h.resume();
        }).detach();
    }

    T await_resume() { return fut.get(); }
};

Task<std::string> async_input(const std::string &prompt);

Task<> human_loop(GameState &gs,
                  Host &host,
                  int human_id,
                  Logger &log,
                  bool day_phase);
