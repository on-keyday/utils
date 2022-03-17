/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#pragma once
#include "../platform/windows/dllexport_header.h"
#include "../thread/lite_lock.h"
#include "../wrap/lite/smart_ptr.h"
#include "task.h"
#include "coroutine/detect.h"

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
            wrap::weak_ptr<Context> ptr;
            ~DefferCancel();
            static wrap::weak_ptr<Context> get_weak(Context*);
        };

        template <class Fn>
        struct Canceler {
            Fn fn;
            wrap::weak_ptr<Context> ptr;
            void operator()(Context& ctx) {
                DefferCancel _{ptr};
                fn(ctx);
            }

            template <class F>
            Canceler(F&& f, Context* ptr)
                : fn(std::move(f)), ptr(DefferCancel::get_weak(ptr)) {}

            Canceler(Canceler&& c)
                : fn(std::move(c.fn)), ptr(std::move(c.ptr)) {
            }
        };

        struct ExternalTask {
           private:
            wrap::weak_ptr<Context> ptr;
            Any param = nullptr;
            friend struct Context;

           public:
            constexpr ExternalTask() {}

            Any& get_param() {
                return param;
            }

            void complete() {
                DefferCancel _{std::move(ptr)};
                param = nullptr;
            }
        };

        struct DLL AnyFuture {
            AnyFuture& wait_until(Context& ctx);
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
#ifdef UTILS_COROUTINE_NAMESPACE
            bool await_ready() {
                return is_done();
            }

            bool await_suspend(coro_ns::coroutine_handle<void> handle) {
                return !is_done();
            }

            Any await_resume() {
                return get();
            }
#endif
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
            constexpr Future() {}
            constexpr Future(std::nullptr_t) {}
            Future(AnyFuture&& p)
                : future(std::move(p)) {}

            Future(T&& t)
                : place(std::forward<T>(t)) {}

            T& get() {
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

            Future& wait_until(async::Context& ctx) {
                future.wait_until(ctx);
                return *this;
            }

#ifdef UTILS_COROUTINE_NAMESPACE
            bool await_ready() {
                return is_done();
            }

            bool await_suspend(coro_ns::coroutine_handle<void> handle) {
                return !is_done();
            }

            T await_resume() {
                return get();
            }
#endif
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

            void force_run_max_thread();

            bool run_on_this_thread();

            template <class Fn>
            void post(Fn&& fn) {
                posting(std::forward<Fn>(fn));
            }

            template <class Fn>
            AnyFuture start(Fn&& fn) {
                return starting(std::forward<Fn>(fn));
            }

            template <class T, class Fn>
            Future<T> start_unwrap(Fn&& fn) {
                return Future<T>{starting(std::move(fn))};
            }

            template <class T, class Fn>
            Future<T> start(Fn&& fn, T defval = T{}) {
                auto f = [fn = std::forward<Fn>(fn), defval = std::move(defval)](auto& ctx) mutable {
                    fn(ctx);
                    if (!ctx.value().template type_assert<T>()) {
                        ctx.set_value(std::move(defval));
                    }
                };
                return start_unwrap<T>(std::move(f));
            }

            ~TaskPool();
        };

        namespace internal {

            template <class Fn, class... Args, size_t... v>
            decltype(auto) call_with_ctx(Context& ctx, Fn&& fn, std::tuple<Args...>&& tup, std::index_sequence<v...>) {
                return fn(ctx, std::move(std::get<v>(tup))...);
            }

            template <class Ret>
            struct AsyncInvoker {
                template <class Fn, class... Args>
                static async::Future<Ret> invoke(TaskPool& p, Fn&& fn, Args&&... arg) {
                    return p.template start_unwrap<Ret>([fn = std::move(fn), tup = std::make_tuple(std::forward<Args>(arg)...)](async::Context& ctx) mutable {
                        ctx.set_value(call_with_ctx(ctx, std::forward<decltype(fn)>(fn), std::move(tup), std::make_index_sequence<sizeof...(Args)>{}));
                    });
                }
            };

            template <>
            struct AsyncInvoker<void> {
                template <class Fn, class... Args>
                static async::AnyFuture invoke(TaskPool& p, Fn&& fn, Args&&... arg) {
                    return p.start([fn = std::move(fn), tup = std::make_tuple(std::forward<Args>(arg)...)](async::Context& ctx) mutable {
                        call_with_ctx(ctx, std::forward<decltype(fn)>(fn), std::move(tup), std::make_index_sequence<sizeof...(Args)>{});
                    });
                }
            };

        }  // namespace internal

        template <class Fn, class... Args>
        decltype(auto) start(TaskPool& p, Fn&& fn, Args&&... args) {
            return internal::AsyncInvoker<std::invoke_result_t<Fn, Context&, Args...>>::invoke(p, fn, std::forward<Args>(args)...);
        }
    }  // namespace async
}  // namespace utils
