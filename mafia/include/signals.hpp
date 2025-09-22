#pragma once

#include <coroutine>
#include <cstddef>


struct PhaseSignal {
    std::size_t epoch = 0;

    struct awaiter {
        PhaseSignal &sig;
        std::size_t seen;

        bool await_ready() const

        noexcept { return sig.epoch > seen; }

        void await_suspend(std::coroutine_handle<>)

        noexcept {}

        void await_resume() const

        noexcept {}
    };

    auto operator co_await()

    noexcept {
        return awaiter{*this, epoch};
    }

    void advance()

    noexcept { ++epoch; }
};
