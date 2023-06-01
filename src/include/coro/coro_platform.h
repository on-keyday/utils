/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "coro.h"
#include "ring.h"
#include <exception>
#include <cstddef>
#include <new>
#include <mutex>

namespace utils {
    namespace coro {
        struct Platform {
            RingQue<C*> running;
            RingQue<C*> idle;
            Resource resource;
            std::exception_ptr except;
            C* current = nullptr;
            std::mutex l;

            bool alloc_que(std::size_t max_running, std::size_t max_idle) {
                auto q = resource.alloc(sizeof(C*) * max_running);
                if (!q) {
                    return false;
                }
                new (q) C* [max_running] {};
                running.set_que(static_cast<C**>(q), max_running);
                q = resource.alloc(sizeof(C*) * max_idle);
                if (!q) {
                    return false;
                }
                new (q) C* [max_idle] {};
                idle.set_que(static_cast<C**>(q), max_idle);
                return true;
            }

           private:
            void fetch_tasks_impl() {
                while (!running.block()) {
                    auto new_task = idle.pop();
                    if (!new_task) {
                        return;
                    }
                    running.push(std::move(new_task));
                }
            }

           public:
            auto lock() {
                l.lock();
                return helper::defer([&] {
                    l.unlock();
                });
            }

            void fetch_tasks() {
                auto l = lock();
                fetch_tasks_impl();
            }

            ~Platform() {
                if (auto got = running.set_que(nullptr, 0)) {
                    resource.free(got);
                }
                if (auto got = idle.set_que(nullptr, 0)) {
                    resource.free(got);
                }
            }
        };
    }  // namespace coro
}  // namespace utils