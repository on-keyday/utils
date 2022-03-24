/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// native_context - low level execution context
#pragma once

namespace utils {
    namespace async {
        namespace light {

#ifdef _WIN32
#define ASYNC_NO_VTABLE_ __declspec(novtable)
#else
#define ASYNC_NO_VTABLE_
#endif
            struct ASYNC_NO_VTABLE_ Executor {
                virtual void execute() = 0;
                virtual ~Executor() {}
            };

            // native context suite

            // low level contextt wrapper
            struct native_context;

            // create native_context
            native_context* create_native_context(Executor* exec, void (*deleter)(Executor*));

            // reset executor
            bool reset_executor(native_context* ctx, Executor* exec, void (*deleter)(Executor*));

            // tell whether ctx is stil running
            bool is_still_running(const native_context* ctx);

            // tell whether ctx is on end_of_function state
            bool is_on_end_of_function(const native_context* ctx);

            // tell whether ctx is not called yet
            bool is_not_called_yet(const native_context* ctx);

            // aquire context internal lock
            // insecure function
            bool acquire_context_lock(native_context* ctx);

            // release context internal lock
            // insecure function
            void release_context_lock(native_context* ctx);

            // wait until lock is acquired
            // insecure function
            void acquire_or_wait_lock(native_context* ctx);

            // invoke executor
            bool invoke_executor(native_context* ctx, bool no_run_if_end);

            // back to caller
            bool return_to_caller(native_context* ctx);

            // delete native_context
            bool delete_native_context(native_context* ctx);

            // switch context
            bool switch_context(native_context* from, native_context* to, bool not_run_on_end);

        }  // namespace light
    }      // namespace async
}  // namespace utils
