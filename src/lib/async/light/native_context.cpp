/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "native_context.h"

namespace utils {
    namespace async {
        namespace light {

            void start_proc(native_context* ctx) {
                ctx->exec->execute();
                ::swapcontext(get_ucontext(ctx->native_handle), ucontext_from(ctx->native_handle));
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

            native_context* create_native_context(Executor* exec, void (*deleter)(Executor*)) {
                if (!exec || !deleter) {
                    return nullptr;
                }
                auto ctx = new native_context{};
                ctx->exec = exec;
                ctx->deleter = deleter;
                ctx->native_handle = nullptr;
                return ctx;
            }

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
                assert(ctx->native_handle);
                auto uctx = get_ucontext(ctx->native_handle);
                if (::getcontext(uctx) != 0) {
                    move_from_stack(ctx->native_handle, stack);
                    ctx->native_handle = nullptr;
                    stack.clean();
                    return false;
                }
                ::makecontext(uctx, (void (*)())start_proc, 1, ctx);
#endif
                return true;
            }

            void swap_ctx(native_handle_t* to, ucontext_t* from) {
                ucontext_from(to) = from;
                ::swapcontext(from, get_ucontext(to));
                ucontext_from(to) = nullptr;
            }

            bool invoke_executor(native_context* ctx) {
                if (!ctx) {
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
                PrepareContext this_;
                swap_ctx(ctx->native_handle, this_.ctx);
                return true;
#endif
            }

            bool switch_context(native_context* from, native_context* to) {
                if (!from || !to) {
                    return false;
                }
                if (!from->native_handle) {
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
                swap_ctx(to->native_handle, curctx);
                return true;
#endif
            }

        }  // namespace light
    }      // namespace async
}  // namespace utils
