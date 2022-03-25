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
                Future<T> task;
                bool run() override {
                    task.resume();
                    return task.done();
                }
            };

            struct TaskPool {
               private:
                LFList<Task*> list;

               public:
                template <class T>
                void append(Future<T>&& f) {
                    auto ftask = new FTask<T>{std::move(f)};
                }
            };

        }  // namespace light
    }      // namespace async
}  // namespace utils
