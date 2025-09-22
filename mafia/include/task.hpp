#pragma once

#include <coroutine>
#include <optional>
#include <utility>
#include <exception>
#include <type_traits>

template<typename T = void>
struct Task {
    struct promise_type {
        std::optional<T> value;
        std::exception_ptr exception;

        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_always initial_suspend() noexcept { return {}; }

        std::suspend_always final_suspend() noexcept { return {}; }

        template<class U>
        void return_value(U &&v) { value = std::forward<U>(v); }

        void unhandled_exception() { exception = std::current_exception(); }
    };

    std::coroutine_handle<promise_type> h{};

    Task() = default;

    explicit Task(std::coroutine_handle<promise_type> hh) : h(hh) {}

    Task(const Task &) = delete;

    Task(Task &&o) noexcept: h(o.h) { o.h = {}; }

    Task &operator=(Task &&o) noexcept {
        if (this != &o) {
            if (h) h.destroy();
            h = o.h;
            o.h = {};
        }
        return *this;
    }

    ~Task() { if (h) h.destroy(); }

    bool done() const { return !h || h.done(); }

    T get() {
        while (!h.done()) h.resume();
        if (h.promise().exception) std::rethrow_exception(h.promise().exception);
        return std::move(*h.promise().value);
    }

    // awaiter общий (используем в трёх перегрузках ниже)
    struct Awaiter {
        Task *self;

        bool await_ready() const noexcept { return !self->h || self->h.done(); }

        bool await_suspend(std::coroutine_handle<>) {
            while (!self->h.done()) self->h.resume();
            return false; // сразу продолжить вызывающую корутину
        }

        T await_resume() {
            if (self->h.promise().exception) std::rethrow_exception(self->h.promise().exception);
            return std::move(*self->h.promise().value);
        }
    };

    auto operator
    co_await() &        { return Awaiter{this}; }
    auto operator
    co_await() const &  { return Awaiter{const_cast<Task *>(this)}; }
    auto operator
    co_await() &&       { return Awaiter{this}; }
};

// Специализация под void
template<>
struct Task<void> {
    struct promise_type {
        std::exception_ptr exception;

        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_always initial_suspend() noexcept { return {}; }

        std::suspend_always final_suspend() noexcept { return {}; }

        void return_void() {}

        void unhandled_exception() { exception = std::current_exception(); }
    };

    std::coroutine_handle<promise_type> h{};

    Task() = default;

    explicit Task(std::coroutine_handle<promise_type> hh) : h(hh) {}

    Task(const Task &) = delete;

    Task(Task &&o) noexcept: h(o.h) { o.h = {}; }

    Task &operator=(Task &&o) noexcept {
        if (this != &o) {
            if (h) h.destroy();
            h = o.h;
            o.h = {};
        }
        return *this;
    }

    ~Task() { if (h) h.destroy(); }

    bool done() const { return !h || h.done(); }

    void get() {
        while (!h.done()) h.resume();
        if (h.promise().exception) std::rethrow_exception(h.promise().exception);
    }

    struct Awaiter {
        Task *self;

        bool await_ready() const noexcept { return !self->h || self->h.done(); }

        bool await_suspend(std::coroutine_handle<>) {
            while (!self->h.done()) self->h.resume();
            return false;
        }

        void await_resume() {
            if (self->h.promise().exception) std::rethrow_exception(self->h.promise().exception);
        }
    };

    auto operator
    co_await() &        { return Awaiter{this}; }
    auto operator
    co_await() const &  { return Awaiter{const_cast<Task *>(this)}; }
    auto operator
    co_await() &&       { return Awaiter{this}; }
};
