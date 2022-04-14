/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// context - light context
#pragma once
#include "make_arg.h"
#include "../../wrap/light/smart_ptr.h"
#include "native_context.h"

namespace utils {
    namespace async {
        namespace light {
            struct ContextBase;

            template <class Ret, class... Other>
            struct ArgExecutor : Executor {
                FuncRecord<Ret, Other...> record;
                constexpr ArgExecutor(FuncRecord<Ret, Other...>&& rec)
                    : record(std::move(rec)) {}

                void execute() override {
                    record.execute();
                }
            };

            template <class T>
            struct SharedContext {
               private:
                native_context* native_ctx = nullptr;
                Args<T>* ret_obj = nullptr;
                bool value_set = false;
                template <class U>
                friend struct SharedContext;

               public:
                void set_value(bool state) {
                    value_set = state;
                }
                using return_value_t = decltype(ret_obj->template get<0>());

                template <class Fn>
                bool get_cb(Fn&& fn, bool inf = false) {
                    if (inf) {
                        acquire_or_wait_lock(native_ctx);
                    }
                    else {
                        if (!acquire_context_lock(native_ctx)) {
                            return false;
                        }
                    }
                    if (!done() && !value_set) {
                        release_context_lock(native_ctx);
                        return false;
                    }
                    fn(ret_obj);
                    release_context_lock(native_ctx);
                    return true;
                }

                bool return_() {
                    return return_to_caller(native_ctx);
                }

                Args<T>* retobj() {
                    return ret_obj;
                }

                bool invoke() {
                    return invoke_executor(native_ctx);
                }

                template <class U>
                bool await_with(wrap::shared_ptr<SharedContext<U>>& ptr) {
                    auto to = ptr->native_ctx;
                    auto from = native_ctx;
                    return switch_context(from, to);
                }

                bool running() const {
                    return is_still_running(native_ctx);
                }

                bool suspended() const {
                    return !running() && !is_on_end_of_function(native_ctx);
                }

                bool done() const {
                    return !is_not_called_yet(native_ctx) && is_on_end_of_function(native_ctx);
                }

                template <class Fn, class... Args>
                bool replace_function(Fn&& fn, Args&&... args) {
                    static_assert(std::is_same_v<invoke_res<Fn, Args...>, T>, "invoke result must be same");
                    auto exec = new ArgExecutor(make_funcrecord(std::forward<Fn>(fn), std::forward<Args>(args)...));
                    if (native_ctx) {
                        if (!reset_executor(
                                native_ctx, exec, [](Executor* e) { delete e; }, control_flag::once)) {
                            delete exec;
                            return false;
                        }
                    }
                    else {
                        native_ctx = create_native_context(
                            exec, [](Executor* e) { delete e; }, control_flag::once);
                    }
                    ret_obj = &exec->record.retobj;
                    return true;
                }

                ~SharedContext() {
                    delete_native_context(native_ctx);
                }
            };

            template <class Fn, class... Args>
            auto make_shared_context(Fn&& fn, Args&&... args) {
                auto ctx = wrap::make_shared<SharedContext<invoke_res<Fn, Args...>>>();
                ctx->replace_function(std::forward<Fn>(fn), std::forward<Args>(args)...);
                return ctx;
            }

            template <class Ret>
            auto make_shared_context() {
                return wrap::make_shared<SharedContext<Ret>>();
            }

            template <class T>
            struct Context;

            template <class Ret, class Fn, class... Arg>
            bool bind_func(wrap::shared_ptr<SharedContext<Ret>>& ctx, Fn&& fn, Arg&&... arg);

            template <class T>
            struct Future {
               private:
                wrap::shared_ptr<SharedContext<T>> ctx;

                template <class Ret, class Fn, class... Args>
                friend Future<Ret> start(bool suspend, Fn&& fn, Args&&... args);
                Future(wrap::shared_ptr<SharedContext<T>> v)
                    : ctx(std::move(v)) {}

                template <class T_, class U>
                friend U await_impl(wrap::shared_ptr<SharedContext<T_>>& ctx, Future<U> f);

               public:
                template <class Fn, class... Args>
                bool rebind_function(Fn&& fn, Args&&... args) {
                    if (!done()) {
                        return false;
                    }
                    return bind_func(ctx, std::forward<Fn>(fn), std::forward<Args>(args)...);
                }

                bool resume() {
                    return ctx->invoke();
                }

                bool suspended() const {
                    return ctx->suspended();
                }

                bool done() const {
                    return ctx->done();
                }

                T get() {
                    Args<T>* ptr;
                    while (true) {
                        if (!ctx->get_cb(
                                [&](Args<T>* p) {
                                    ptr = p;
                                },
                                true)) {
                            resume();
                            continue;
                        }
                        break;
                    }
                    return ptr->template get<0>();
                }
            };

            template <class T, class U>
            U await_impl(wrap::shared_ptr<SharedContext<T>>& ctx, Future<U> u) {
                Args<U>* ptr;
                while (true) {
                    if (!u.ctx->get_cb([&](Args<U>* p) {
                            ptr = p;
                        })) {
                        if (!ctx->await_with(u.ctx)) {
                            ctx->return_();
                        }
                    }
                    else {
                        break;
                    }
                }
                return ptr->template get<0>();
            }

            template <class Ret, class Fn, class... Args>
            Future<Ret> start(bool suspend, Fn&& fn, Args&&... args) {
                auto ctx = make_shared_context<Ret>();
                bind_func(ctx, std::forward<Fn>(fn), std::forward<Args>(args)...);
                if (!suspend) {
                    ctx->invoke();
                }
                return Future<Ret>{ctx};
            }

            template <class Ret, class Fn, class... Args>
            Future<Ret> invoke(Fn&& fn, Args&&... args) {
                return start<Ret>(false, std::forward<Fn>(fn), std::forward<Args>(args)...);
            }

            template <class T>
            struct Context {
               private:
                template <class Ret, class Fn, class... Arg>
                friend bool bind_func(wrap::shared_ptr<SharedContext<Ret>>& ctx, Fn&& fn, Arg&&... arg);

                wrap::shared_ptr<SharedContext<T>> ctx;

                Context(wrap::shared_ptr<SharedContext<T>> v)
                    : ctx(std::move(v)) {}

               public:
                bool suspend() {
                    return ctx->return_();
                }

                bool yield(T t) {
                    *ctx->retobj() = make_arg(std::forward<T>(t));
                    ctx->set_value(true);
                    auto res = suspend();
                    ctx->set_value(false);
                    return res;
                }

                template <class U>
                U await(Future<U> u) {
                    return await_impl(ctx, u);
                }

                template <class Ret, class Fn, class... Args>
                decltype(auto) await(Fn&& fn, Args&&... args) {
                    return await(start<Ret>(true, std::forward<Fn>(fn), std::forward<Args>(args)...));
                }
            };

            template <>
            struct Context<void> {
               private:
                template <class Ret, class Fn, class... Arg>
                friend bool bind_func(wrap::shared_ptr<SharedContext<Ret>>& ctx, Fn&& fn, Arg&&... arg);

                wrap::shared_ptr<SharedContext<void>> ctx;

                Context(wrap::shared_ptr<SharedContext<void>> v)
                    : ctx(std::move(v)) {}

               public:
                bool suspend() {
                    return ctx->return_();
                }

                template <class U>
                U await(Future<U> u) {
                    return await_impl(ctx, u);
                }

                template <class Ret, class Fn, class... Args>
                decltype(auto) await(Fn&& fn, Args&&... args) {
                    return await(start<Ret>(true, std::forward<Fn>(fn), std::forward<Args>(args)...));
                }
            };

            template <class Ret, class Fn, class... Arg>
            bool bind_func(wrap::shared_ptr<SharedContext<Ret>>& ctx, Fn&& fn, Arg&&... arg) {
                if constexpr (std::is_invocable_r_v<Ret, Fn, Context<Ret>, Arg...>) {
                    return ctx->replace_function(std::forward<Fn>(fn), Context<Ret>{ctx}, std::forward<Arg>(arg)...);
                }
                else {
                    return ctx->replace_function(std::forward<Fn>(fn), std::forward<Arg>(arg)...);
                }
            }

        }  // namespace light
    }      // namespace async
}  // namespace utils
