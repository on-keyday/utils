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

        template <class Fn>
        struct Canceler {
            Fn fn;
            Context* ptr = nullptr;
            void operator()(Context& ctx) const {
                fn(ctx);
                ptr->set_signal();
            }

            Canceler(Fn&& fn, Context* ptr)
                : fn(std::move(fn)), ptr(ptr) {}

            Canceler(Canceler&& c) {
                fn = std::move(c.fn);
                ptr = c.ptr;
                c.ptr = nullptr;
            }

            ~Canceler() {
                if (ptr)
                    ptr->set_signal();
            }
        };

        struct DLL Context {
            template <class Fn>
            friend struct Canceler;
            void suspend();
            void cancel();

            void set_value(Any any);

            template <class Fn>
            bool wait_task(Fn&& fn) {
                Task<Context> c = Canceler<std::decay_t<Fn>>{std::move(fn), this};
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

        struct DLL TaskPool {
           private:
            thread::LiteLock initlock;
            wrap::shared_ptr<internal::WorkerData> data;
            void posting(Task<Context>&& task);

           public:
            template <class Fn>
            void post(Fn&& fn) {
                posting(std::forward<Fn>(fn));
            }
        };

    }  // namespace async
}  // namespace utils
