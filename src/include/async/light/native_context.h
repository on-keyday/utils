/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// native_context - low level execution context
#pragma once
#include "../../platform/windows/dllexport_header.h"
#include <wrap/light/enum.h>
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

            // native context behaviour control flag
            enum class control_flag {
                none = 0,
                once = 1,     // once reaching end of function, don't run again
                rethrow = 2,  // if on end of function and exception exists, rethrow
            };

            DEFINE_ENUM_FLAGOP(control_flag);

            // create native_context
            DLL native_context* STDCALL create_native_context(Executor* exec, void (*deleter)(Executor*), control_flag flag);

            // reset executor
            DLL bool STDCALL reset_executor(native_context* ctx, Executor* exec, void (*deleter)(Executor*), control_flag flag);

            // tell whether ctx is stil running
            DLL bool STDCALL is_still_running(const native_context* ctx);

            // tell whether ctx is on end_of_function state
            DLL bool STDCALL is_on_end_of_function(const native_context* ctx);

            // tell whether ctx is not called yet
            DLL bool STDCALL is_not_called_yet(const native_context* ctx);

            // aquire context internal lock
            // insecure function
            DLL bool STDCALL acquire_context_lock(native_context* ctx);

            // release context internal lock
            // insecure function
            DLL void STDCALL release_context_lock(native_context* ctx);

            // wait until lock is acquired
            // insecure function
            DLL void STDCALL acquire_or_wait_lock(native_context* ctx);

            // invoke executor
            DLL bool STDCALL invoke_executor(native_context* ctx);

            // return current context to caller
            DLL bool STDCALL return_to_caller(native_context* ctx);

            // delete native_context
            DLL bool STDCALL delete_native_context(native_context* ctx);

            // switch context
            DLL bool STDCALL switch_context(native_context* from, native_context* to);

        }  // namespace light
    }      // namespace async
}  // namespace utils
