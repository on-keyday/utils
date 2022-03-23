/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "native_context_impl.h"

namespace utils {
    namespace async {
        namespace light {

            void start_proc(native_context* ctx) {
                while (true) {
                    try {
                        ctx->exec->execute();
                    } catch (...) {
                        ctx->exception = std::current_exception();
                    }
                    ctx->end_of_function = true;
                    return_to_caller(ctx);
                }
            }

            struct PrepareContext {
#ifdef _WIN32
#else
                ucontext_t* ctx;

                PrepareContext() {
                    ctx = new ucontext_t{};
                    auto res = ::getcontext(ctx);
                    assert(res == 0);
                }

                ~PrepareContext() {
                    delete ctx;
                }
#endif
            };

            struct GetOwnership {
                std::atomic_flag& flag;
                bool result;
                GetOwnership(std::atomic_flag& f)
                    : flag(f) {
                    result = flag.test_and_set();
                }

                operator bool() {
                    return result == false;
                }

                ~GetOwnership() {
                    if (result == false) {
                        flag.clear();
                    }
                }
            };

            bool aquire_context_lock(native_context* ctx) {
                if (!ctx) {
                    return false;
                }
                GetOwnership own(ctx->run);
                if (own) {
                    own.result = true;
                    return true;
                }
                return false;
            }

            void release_context_lock(native_context* ctx) {
                if (!ctx || !ctx->run.test()) {
                    return;
                }
                ctx->run.clear();
            }

            bool is_still_running(const native_context* ctx) {
                if (!ctx) {
                    return true;
                }
                return ctx->run.test();
            }

            bool is_on_end_of_function(const native_context* ctx) {
                if (!ctx) {
                    return false;
                }
                return ctx->end_of_function;
            }

            native_context* create_native_context(Executor* exec, void (*deleter)(Executor*)) {
                if (exec && !deleter || !exec && deleter) {
                    return nullptr;
                }
                auto ctx = new native_context{};
                ctx->exec = exec;
                ctx->deleter = deleter;
                ctx->native_handle = nullptr;
                ctx->end_of_function = true;
                return ctx;
            }

            bool reset_executor(native_context* ctx, Executor* exec, void (*deleter)(Executor*)) {
                if (!ctx || exec && !deleter || !exec && deleter) {
                    return false;
                }
                GetOwnership own(ctx->run);
                if (!own) {
                    return false;
                }
                if (!ctx->end_of_function) {
                    return false;
                }
                if (ctx->exec) {
                    ctx->deleter(ctx->exec);
                }
                ctx->exec = exec;
                ctx->deleter = deleter;
                return true;
            }
#ifdef __linux__
            static bool reset_ucontext(native_context* ctx) {
                assert(ctx->native_handle);
                auto uctx = get_ucontext(ctx->native_handle);
                if (::getcontext(uctx) != 0) {
                    return false;
                }
                set_stack_pointer(ctx->native_handle);
                ::makecontext(uctx, (void (*)())start_proc, 1, ctx);
                return true;
            }
#endif

            static bool initialize_context(native_context* ctx) {
                if (ctx->native_handle) {
                    return true;
                }
#ifdef _WIN32
#else
                native_stack stack;
                if (!stack.init()) {
                    return false;
                }
                ctx->native_handle = move_to_stack(stack);
                if (!reset_ucontext(ctx)) {
                    move_from_stack(ctx->native_handle, stack);
                    ctx->native_handle = nullptr;
                    stack.clean();
                }
#endif
                return true;
            }

            static void swap_ctx(native_handle_t* to, ucontext_t* from) {
                ucontext_from(to) = from;
                ::swapcontext(from, get_ucontext(to));
                ucontext_from(to) = nullptr;
            }

            bool return_to_caller(native_context* ctx) {
                if (!ctx) {
                    return false;
                }
                auto caller = ucontext_from(ctx->native_handle);
                auto callee = get_ucontext(ctx->native_handle);
                if (!caller) {
                    return false;
                }
                assert(callee);
                ::swapcontext(callee, caller);
                return true;
            }

            bool invoke_executor(native_context* ctx) {
                if (!ctx) {
                    return false;
                }
                GetOwnership own(ctx->run);
                if (!own) {
                    return false;
                }
                if (!ctx->exec) {
                    return false;
                }
                if (!initialize_context(ctx)) {
                    return false;
                }
#ifdef _WIN32
#else
                if (ucontext_from(ctx->native_handle)) {
                    return false;
                }
                ctx->end_of_function = false;
                PrepareContext this_;
                swap_ctx(ctx->native_handle, this_.ctx);
                return true;
#endif
            }

            bool switch_context(native_context* from, native_context* to) {
                if (!from || !to) {
                    return false;
                }
                GetOwnership own(to->run);
                if (!own) {
                    return false;
                }
                if (!from->native_handle || !from->run.test()) {
                    return false;
                }
                if (!to->native_handle) {
                    if (!initialize_context(to)) {
                        return false;
                    }
                }
#ifdef _WIN32
#else
                if (ucontext_from(to->native_handle)) {
                    return false;
                }
                auto curctx = get_ucontext(from->native_handle);
                to->end_of_function = false;
                swap_ctx(to->native_handle, curctx);
                return true;
#endif
            }

            bool delete_native_context(native_context* ctx) {
                if (!ctx) {
                    return true;
                }
                GetOwnership own(ctx->run);
                if (!own) {
                    return false;
                }
                if (!ctx->end_of_function) {
                    return false;
                }
                if (ctx->exec) {
                    ctx->deleter(ctx->exec);
                }
                if (ctx->native_handle) {
#ifdef _WIN32
#else
                    native_stack stack;
                    move_from_stack(ctx->native_handle, stack);
                    stack.clean();
#endif
                }
                delete ctx;
                own.result = true;
                return true;
            }

        }  // namespace light
    }      // namespace async
}  // namespace utils
