/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#pragma once
#include <thread>
#include "../platform/windows/dllexport_header.h"
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

        struct Context;
        struct DefferCancel;
        struct DLL Canceler {
            Task<Context> fn;
            Context* ptr = nullptr;
            void operator()(Context& ctx) const;

            template <class Fn>
            Canceler(Fn&& fn, Context* ptr)
                : fn(std::move(fn)), ptr(ptr) {}

            Canceler(Canceler&& c) {
                fn = std::move(c.fn);
                ptr = c.ptr;
                c.ptr = nullptr;
            }
        };

        struct DLL Context {
            friend struct DefferCancel;
            void suspend();
            void cancel();

            void set_value(Any any);

            template <class Fn>
            bool wait_task(Fn&& fn) {
                Task<Context> c = Canceler{std::move(fn), this};
                return wait_task(std::move(c));
            }

           private:
            friend struct internal::ContextHandle;
            wrap::shared_ptr<internal::ContextData> data;
            bool wait_task(Task<Context>&& task);
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

        struct TaskPool;

        struct Future {
           private:
            wrap::shared_ptr<internal::ContextData> data;
            friend struct TaskPool;
        };

        struct DLL TaskPool {
           private:
            thread::LiteLock initlock;
            wrap::shared_ptr<internal::WorkerData> data;
            void init();
            void posting(Task<Context>&& task);
            Future starting(Task<Context>&& task);

           public:
            template <class Fn>
            void post(Fn&& fn) {
                posting(std::forward<Fn>(fn));
            }

            template <class Fn>
            Future start(Fn&& fn) {
                startting(std::forward<Fn>(fn));
            }

            ~TaskPool();
        };

    }  // namespace async
}  // namespace utils
