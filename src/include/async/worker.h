/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#pragma once
#include <thread>
#include "../thread/lite_lock.h"
#include "../wrap/lite/smart_ptr.h"
#include "task.h"

namespace utils {
    namespace async {

        namespace internal {
            struct WorkerData;
            struct ContextData;
            struct ContextHandle;
        }  // namespace internal

        struct Context {
            void suspend();
            void cancel();

            template <class Fn>
            bool wait_task(Fn&& fn) {
                Task<void, Context> c = [this, fn = std::move(fn)](auto& ctx) {
                    fn(ctx);
                    this->set_signal();
                };
                wait_task(c);
            }

           private:
            friend struct internal::ContextHandle;
            wrap::shared_ptr<internal::ContextData> data;
            bool wait_task(Task<void, Context>&& task);
            void set_signal();
        };

        enum class TaskState {
            prelaunch,
            running,
            suspend,
            wait_signal,
            done,
            except,
            cahceled,
        };

        struct TaskPool {
           private:
            thread::LiteLock initlock;
            wrap::shared_ptr<internal::WorkerData> data;
            void posting(Task<Any, Context>&& task);

           public:
            template <class Fn>
            void post(Fn&& fn) {
                posting(std::forward<Fn>(fn));
            }
        };

    }  // namespace async
}  // namespace utils
