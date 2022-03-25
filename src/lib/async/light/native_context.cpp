/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../../include/platform/windows/dllexport_source.h"
#include "native_context_impl.h"

namespace utils {
    namespace async {
        namespace light {

            void STDCALL start_proc(native_context* ctx) {
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
                void* fiber;
                bool cvt = false;
                PrepareContext() {
                    if (::IsThreadAFiber()) {
                        fiber = ::GetCurrentFiber();
                    }
                    else {
                        fiber = ::ConvertThreadToFiber(nullptr);
                        cvt = true;
                    }
                    assert(fiber);
                }

                ~PrepareContext() {
                    if (cvt) {
                        ::ConvertFiberToThread();
                    }
                }
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

                void set_dangling_lock() {
                    result = true;
                }

                ~GetOwnership() {
                    if (result == false) {
                        flag.clear();
                        flag.notify_all();
                    }
                }
            };

            bool acquire_context_lock(native_context* ctx) {
                if (!ctx) {
                    return false;
                }
                GetOwnership own(ctx->run);
                if (own) {
                    own.set_dangling_lock();
                    return true;
                }
                return false;
            }

            void acquire_or_wait_lock(native_context* ctx) {
                while (!acquire_context_lock(ctx)) {
                    ctx->run.wait(true);
                }
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

            bool is_not_called_yet(const native_context* ctx) {
                if (!ctx) {
                    return true;
                }
                return ctx->first_time;
            }

            native_context* create_native_context(Executor* exec, void (*deleter)(Executor*), control_flag flag) {
                if (exec && !deleter || !exec && deleter) {
                    return nullptr;
                }
                auto ctx = new native_context{};
                ctx->exec = exec;
                ctx->deleter = deleter;
                ctx->native_handle = nullptr;
#ifdef _WIN32
                ctx->fiber_from = nullptr;
#endif
                ctx->end_of_function = true;
                ctx->first_time = true;
                ctx->flag = flag;
                return ctx;
            }

            bool reset_executor(native_context* ctx, Executor* exec, void (*deleter)(Executor*), control_flag flag) {
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
                ctx->first_time = true;
                ctx->flag = flag;
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
                ctx->native_handle = ::CreateFiberEx(1024 * 64, 2048 * 64, 0, (LPFIBER_START_ROUTINE)start_proc, ctx);
                if (!ctx->native_handle) {
                    return false;
                }
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

#ifdef _WIN32
            static void swap_ctx(native_context* ctx, void* from) {
                ctx->fiber_from = from;
                ::SwitchToFiber(ctx->native_handle);
                ctx->fiber_from = nullptr;
            }
#else
            static void swap_ctx(native_handle_t* to, ucontext_t* from) {
                ucontext_from(to) = from;
                ::swapcontext(from, get_ucontext(to));
                ucontext_from(to) = nullptr;
            }
#endif
            bool return_to_caller(native_context* ctx) {
                if (!ctx) {
                    return false;
                }
#ifdef _WIN32
                if (!ctx->fiber_from) {
                    return false;
                }
                assert(ctx->native_handle);
                ::SwitchToFiber(ctx->fiber_from);
#else
                auto caller = ucontext_from(ctx->native_handle);
                auto callee = get_ucontext(ctx->native_handle);
                if (!caller) {
                    return false;
                }
                assert(callee);
                ::swapcontext(callee, caller);
#endif
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
                if (any(ctx->flag & control_flag::once) && !ctx->first_time && ctx->end_of_function) {
                    return false;
                }
#ifdef _WIN32
                if (ctx->fiber_from) {
                    return false;
                }
                ctx->first_time = false;
                ctx->end_of_function = false;
                PrepareContext this_;
                swap_ctx(ctx, this_.fiber);
#else
                if (ucontext_from(ctx->native_handle)) {
                    return false;
                }
                ctx->first_time = false;
                ctx->end_of_function = false;
                PrepareContext this_;
                swap_ctx(ctx->native_handle, this_.ctx);

#endif
                return true;
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
                if (any(to->flag & control_flag::once) && !to->first_time && to->end_of_function) {
                    return false;
                }
#ifdef _WIN32
                if (to->fiber_from) {
                    return false;
                }
                auto curfiber = from->native_handle;
                to->end_of_function = false;
                to->first_time = false;
                swap_ctx(to, curfiber);
#else
                if (ucontext_from(to->native_handle)) {
                    return false;
                }
                auto curctx = get_ucontext(from->native_handle);
                to->end_of_function = false;
                to->first_time = false;
                swap_ctx(to->native_handle, curctx);
#endif
                return true;
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
                    PrepareContext this_;
                    ::DeleteFiber(ctx->native_handle);
#else
                    native_stack stack;
                    move_from_stack(ctx->native_handle, stack);
                    stack.clean();
#endif
                }
                delete ctx;
                own.set_dangling_lock();
                return true;
            }

        }  // namespace light
    }      // namespace async
}  // namespace utils
