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

               public:
                Args<T>* ret_obj = nullptr;

                bool invoke() {
                    if (!native_ctx) {
                        return false;
                    }
                    return invoke_executor(native_ctx);
                }

                template <class Fn, class... Args>
                bool replace_function(Fn&& fn, Args&&... args) {
                    static_assert(std::is_same_v<invoke_res<Fn, Args...>, T>, "invoke result must be same");
                    auto exec = new ArgExecutor(make_funcrecord(std::forward<Fn>(fn), std::forward<Args>(args)...));
                    ret_obj = &exec->record.retobj;
                    if (native_ctx) {
                        reset_executor(native_ctx, exec, [](Executor* e) { delete e; });
                    }
                    else {
                        native_ctx = create_native_context(exec, [](Executor* e) { delete e; });
                    }
                    return true;
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

            struct ContextBase {
               protected:
                constexpr ContextBase() {}

                bool suspend(native_context* current);
            };

            template <class T>
            struct Future {
               private:
                wrap::shared_ptr<SharedContext<T>> ctx;

               public:
            };

            template <class T>
            struct Context : ContextBase {
               private:
                wrap::shared_ptr<SharedContext<T>> ctx;

               public:
                template <class Fn, class... Args>
                constexpr Context(Fn&& fn, Args&&... args) {
                }

                bool yield(T t) {
                    *ctx->retobj = make_arg(t);
                    return suspend();
                }

                template <class U>
                U await(Future<U> u) {
                }
            };

        }  // namespace light
    }      // namespace async
}  // namespace utils
