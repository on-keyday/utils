/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// task - task object with cpp coroutine
#pragma once
#include "detect.h"

namespace utils {
    namespace async {
        namespace coro {
#ifdef UTILS_COROUTINE_NAMESPACE
            template <class T>
            struct Task {
                struct promise_type {
                    T value_;
                    auto get_return_object() {
                        return Task{*this};
                    }
                    auto initial_suspend() {
                        return coro_ns::suspend_never{};
                    }
                    auto final_suspend() noexcept {
                        return coro_ns::suspend_always{};
                    }
                    void return_value(T x) {
                        value_ = x;
                    }
                    void unhandled_exception() {
                        std::terminate();
                    }
                };

                T get() {
                    if (!handle.done()) {
                        handle.resume();
                    }
                    return handle.promise().value_;
                }

                Task(Task const&) = delete;
                Task(Task&& in)
                    : handle(std::exchange(in.handle, nullptr)) {}

               private:
                explicit Task(promise_type& p)
                    : handle(coro_ns::coroutine_handle<promise_type>::from_promise(p)) {}

                coro_ns::coroutine_handle<promise_type> handle;
            };
#endif
        }  // namespace coro
    }      // namespace async
}  // namespace utils
