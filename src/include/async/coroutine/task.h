/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// task - task object with cpp coroutine
#pragma once
#include <coroutine>

namespace utils {
    namespace async {
        namespace coro {
            template <class T>
            struct Task {
                struct promise_type {
                    T value_;
                    auto get_return_object() {
                        return Task{*this};
                    }
                    auto initial_suspend() {
                        return std::suspend_never{};
                    }
                    auto final_suspend() noexcept {
                        return std::suspend_always{};
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
                    : handle(std::coroutine_handle<promise_type>::from_promise(p)) {}

                std::coroutine_handle<promise_type> handle;
            };
        }  // namespace coro
    }      // namespace async
}  // namespace utils
