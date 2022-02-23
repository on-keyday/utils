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

        enum class TaskState {
            invalid,
            prelaunch,
            running,
            suspend,
            wait_signal,
            done,
            except,
            canceled,
            term,
        };

        struct Context;
        struct DLL DefferCancel {
            Context* ptr;
            ~DefferCancel();
        };

        template <class Fn>
        struct Canceler {
            Fn fn;
            Context* ptr = nullptr;
            void operator()(Context& ctx) const {
                DefferCancel _{ptr};
                fn(ctx);
            }

            template <class F>
            Canceler(F&& f, Context* ptr)
                : fn(std::move(f)), ptr(ptr) {}

            Canceler(Canceler&& c)
                : fn(std::move(c.fn)) {
                ptr = c.ptr;
                c.ptr = nullptr;
            }
        };

        struct ExternalTask {
           private:
            Context* ptr = nullptr;
            Any param = nullptr;
            friend struct Context;

           public:
            constexpr ExternalTask() {}

            Any& get_param() {
                return param;
            }

            void complete() {
                DefferCancel _{ptr};
                ptr = nullptr;
                param = nullptr;
            }
        };

        struct DLL AnyFuture {
            void wait_or_suspend(Context& ctx);
            void wait();
            Any get();

            TaskState state() const;

            ExternalTask* get_taskrequest();

            bool cancel();

            bool is_done() const {
                auto st = state();
                return st == TaskState::done || st == TaskState::except ||
                       st == TaskState::canceled || st == TaskState::invalid;
            }

            void clear() {
                data = nullptr;
            }

           private:
            wrap::shared_ptr<internal::ContextData> data;
            bool not_own = false;
            friend struct TaskPool;
            friend struct Context;
        };

        template <class T>
        struct Future {
           private:
            AnyFuture future;
            Any place;

           public:
            constexpr Future(std::nullptr_t) {}
            Future(AnyFuture&& p)
                : future(std::move(p)) {}

            T get() {
                if (!place) {
                    place = future.get();
                    future.clear();
                    if (!place) {
                        place = T{};
                    }
                }
                return *place.template type_assert<T>();
            }

            void wait() {
                future.wait();
            }

            bool is_done() const {
                return future.is_done();
            }

            TaskState state() const {
                return future.state();
            }

            void wait_or_suspend(async::Context& ctx) {
                future.wait_or_suspend(ctx);
            }
        };

        struct DLL Context {
            friend struct DefferCancel;
            void suspend();
            void cancel();

            void set_value(Any any);

            Any& value();

            template <class Fn>
            bool task_wait(Fn&& fn) {
                Task<Context> c = Canceler<std::decay_t<Fn>>(std::move(fn), this);
                return task_wait(std::move(c));
            }

            void externaltask_wait(Any param = nullptr);

            AnyFuture clone() const;

            size_t priority() const;

            void set_priority(size_t e);

           private:
            friend struct internal::ContextHandle;
            wrap::shared_ptr<internal::ContextData> data;
            bool task_wait(Task<Context>&& task);
            void set_signal();
        };

        struct DLL TaskPool {
           private:
            thread::LiteLock initlock;
            wrap::shared_ptr<internal::WorkerData> data;
            void init_data();
            void init_thread();
            void init();
            void posting(Task<Context>&& task);
            AnyFuture starting(Task<Context>&& task);

           public:
            size_t reduce_thread();

            void set_maxthread(size_t sz);

            void set_yield(bool do_flag);

            void set_priority_mode(bool priority_mode);

            template <class Fn>
            void post(Fn&& fn) {
                posting(std::forward<Fn>(fn));
            }

            template <class Fn>
            AnyFuture start(Fn&& fn) {
                return starting(std::forward<Fn>(fn));
            }

            template <class T, class Fn>
            Future<T> start(Fn&& fn) {
                auto f = [fn = std::forward<Fn>(fn)](auto& ctx) mutable {
                    fn(ctx);
                    if (!ctx.value().template type_assert<T>()) {
                        ctx.set_value(T{});
                    }
                };
                auto fu = starting(std::move(f));
                return Future<T>{std::move(fu)};
            }

            ~TaskPool();
        };

    }  // namespace async
}  // namespace utils
