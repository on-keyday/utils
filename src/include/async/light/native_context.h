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

            struct native_context;

            // create native_context
            native_context* create_native_context(Executor* exec, void (*deleter)(Executor*));

            // reset executor
            bool reset_executor(native_context* ctx, Executor* exec, void (*deleter)(Executor*));

            // tell for ctx to be still running
            bool is_still_running(const native_context* ctx);

            // tell for ctx to be on end_of_function state
            bool is_on_end_of_function(const native_context* ctx);

            // invoke executor
            bool invoke_executor(native_context* ctx);

            // back to caller
            bool return_to_caller(native_context* ctx);

            // delete native_context
            void delete_native_context(native_context* ctx);
            bool switch_context(native_context* from, native_context* to);

        }  // namespace light
    }      // namespace async
}  // namespace utils
