/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// pool -  task pool object
#pragma once
#include "context.h"
#include <atomic>
#include <cassert>
#include "lock_free_list.h"
#include "../../testutil/timer.h"

namespace utils {
    namespace async {
        namespace light {
            struct ASYNC_NO_VTABLE_ Task {
                virtual bool run() = 0;
                constexpr virtual ~Task() {}
            };

            template <class T>
            struct FTask : Task {
                FTask(Future<T>&& t)
                    : task(std::move(t)) {}
                Future<T> task;
                bool run() override {
                    task.resume();
                    return task.done();
                }
            };

            template <class Dur = std::chrono::milliseconds>
            struct TimeContext {
                test::Timer timer;
                size_t hit = 0;
                size_t not_hit = 0;
                Dur update_delta{};

                size_t prev_hit = 0;
                size_t prev_not_hit = 0;

                Dur wait{};
                Dur limit{};

                void reset_prev() noexcept {
                    prev_hit = hit;
                    prev_not_hit = not_hit;
                }

                size_t hit_delta() const noexcept {
                    return hit - prev_hit;
                }

                size_t not_hit_delta() const noexcept {
                    return not_hit - prev_not_hit;
                }
            };

            struct TaskPool {
               private:
                LFList<Task*> list;
                std::atomic_size_t size_;
                std::atomic_size_t element_count;
                std::atomic_size_t total_insertion;

               public:
                template <class T>
                void append(Future<T>&& f) {
                    auto ftask = new FTask<T>{std::move(f)};
                    size_++;
                    total_insertion++;
                    if (insert_element(&list, static_cast<Task*>(ftask))) {
                        element_count++;
                    }
                }

                bool run_task(SearchContext<Task*>* sctx = nullptr, int search_cost = 1) {
                    auto task = get_a_task(&list, sctx, search_cost);
                    if (!task) {
                        return false;
                    }
                    if (task->run()) {
                        delete task;
                        size_--;
                    }
                    else {
                        total_insertion++;
                        if (insert_element(&list, task, sctx)) {
                            element_count++;
                        }
                    }
                    return true;
                }

                size_t size() const {
                    return size_;
                }

                bool clear() {
                    if (size_) {
                        return false;
                    }
                    list.clear();
                    element_count = 0;
                    return true;
                }

                template <class Dur, class Callback>
                bool run_task_with_wait(TimeContext<Dur>& tctx, Callback&& cb, SearchContext<Task*>* sctx = nullptr, int search_cost = 1) {
                    auto b = run_task(sctx, search_cost);
                    if (!b) {
                        tctx.not_hit++;
                        std::this_thread::sleep_for(tctx.wait);
                        if (tctx.update_delta <= tctx.timer.template delta<Dur>()) {
                            cb(tctx);
                            tctx.reset_prev();
                            tctx.timer.reset();
                        }
                    }
                    else {
                        tctx.hit++;
                    }
                    return b;
                }
            };

        }  // namespace light
    }      // namespace async
}  // namespace utils
