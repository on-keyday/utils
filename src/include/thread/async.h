/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// like Rust's async/await
// see also Rust's std::task::Poll, std::task::Context, std::task::Waker and std::task::AtomicWaker
#pragma once
#include <concepts>
#include <atomic>
#include <optional>
#include <variant>
#include <helper/expected.h>
#include "poll.h"

namespace futils::thread::poll {

    struct VTable {
        void (*drop)(void* self) = nullptr;
        void* (*clone)(void* self) = nullptr;
        void (*wake)(void* self) = nullptr;
        void (*wake_by_ref)(void* self) = nullptr;
    };

    struct Waker {
       private:
        void* data = nullptr;
        const VTable* vtable = nullptr;

       public:
        constexpr Waker() = default;
        constexpr Waker(void* data, const VTable* vtable)
            : data(data), vtable(vtable) {}

        constexpr Waker(Waker&& other) noexcept {
            data = other.data;
            vtable = other.vtable;
            other.data = nullptr;
            other.vtable = nullptr;
        }

        constexpr Waker& operator=(Waker&& other) noexcept {
            if (this != &other) {
                if (vtable && vtable->drop) {
                    vtable->drop(data);
                }
                data = other.data;
                vtable = other.vtable;
                other.data = nullptr;
                other.vtable = nullptr;
            }
            return *this;
        }

        constexpr void wake() const {
            if (vtable && vtable->wake) {
                vtable->wake(data);
            }
        }

        constexpr void wake_by_ref() const {
            if (vtable && vtable->wake_by_ref) {
                vtable->wake_by_ref(data);
            }
        }

        constexpr Waker clone() const {
            if (vtable && vtable->clone) {
                return Waker(vtable->clone(data), vtable);
            }
            return {};
        }

        constexpr ~Waker() {
            if (vtable && vtable->drop) {
                vtable->drop(data);
            }
        }
    };

    struct Context {
        Waker waker;
    };

    template <class T, class R>
    concept PollResult = requires {
        { typename T::Pending{} } -> std::convertible_to<Poll<R>>;
    };

    template <class T, class R>
    concept Poller = requires(T r, Context& ctx) {
        { r.poll(ctx) } -> PollResult<R>;
    };

    namespace test {
        struct StubPoller {
            Poll<int> poll(Context&) {
                return Poll<int>::Pending{};
            }
        };

        static_assert(Poller<StubPoller, int>);
    }  // namespace test

    template <class T>
    concept WakerRegister = requires(T t) {
        { t.register_waker(Waker{}) } -> std::same_as<void>;
    };

    struct AtomicWaker {
       private:
        enum State : std::uint8_t {
            wait = 0,
            waking = 1,
            registering = 2,
        };
        std::atomic<std::uint8_t> state;
        Waker waker;

       public:
        constexpr AtomicWaker() = default;

        void register_waker(const Waker& waker) {
            std::uint8_t expected = wait;
            if (state.compare_exchange_strong(expected, registering)) {
                this->waker = waker.clone();
                state.store(wait);
                return;
            }
            if (expected == waking) {
                waker.wake_by_ref();
            }
        }

        std::optional<Waker> take() {
            auto old = state.fetch_or(waking);
            if (old == wait) {
                auto result = std::move(waker);
                state.fetch_and(~waking);
                return result;
            }
            return std::nullopt;
        }

        void wake() {
            if (auto w = take()) {
                w->wake();
            }
        }
    };

    struct EmptyError {};
    template <class F, class R>
        requires Poller<F, R>
    struct Task {
       private:
        TaskState<F, R> state;

       public:
        Poll<R> poll(Context& ctx) {
            switch (state.get_tag()) {
                case TaskStateTag::Running: {
                    Poll<R> r = state.running()->poll(ctx);
                    if (r.pending()) {
                        return typename Poll<R>::Pending{};
                    }
                    state = typename TaskState<F, R>::Consumed{};
                    return r;
                }
            }
            __builtin_unreachable();
            throw EmptyError{};
        }

        void set_result(Result<R, error::Error<>>&& val) {
            state = typename TaskState<F, R>::Finished{std::move(val)};
        }

        Result<R, error::Error<>> take_result() {
            switch (state.get_tag()) {
                case TaskStateTag::Finished: {
                    auto res = std::move(*state.finished());
                    state = typename TaskState<F, R>::Consumed{};
                    return res;
                }
            }
            __builtin_unreachable();
            throw EmptyError{};
        }
    };

    namespace test {

    }

    template <class Q>
    struct TaskQueue {};

}  // namespace futils::thread::poll
