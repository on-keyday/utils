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

            struct TaskPool {
               private:
                LFList<Task*> list;
                std::atomic_size_t size_;
                std::atomic_size_t element_count;

               public:
                template <class T>
                void append(Future<T>&& f) {
                    auto ftask = new FTask<T>{std::move(f)};
                    size_++;
                    if (insert_element(&list, static_cast<Task*>(ftask))) {
                        element_count++;
                    }
                }

                bool run_task() {
                    auto task = get_a_task(&list);
                    if (!task) {
                        return false;
                    }
                    if (task->run()) {
                        delete task;
                        size_--;
                    }
                    else {
                        if (insert_element(&list, task)) {
                            element_count++;
                        }
                    }
                    return true;
                }

                size_t size() const {
                    return size_;
                }
            };

        }  // namespace light
    }      // namespace async
}  // namespace utils
