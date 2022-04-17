/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// native_context_impl - native context impl
#pragma once
#include "../../../include/async/light/native_context.h"
#include <cstdint>
#include <atomic>
#ifdef _WIN32
#include <Windows.h>
#include <exception>
#include <cassert>
#else
#include "native_stack.h"
#endif

namespace utils {
    namespace async {
        namespace light {

            struct native_context {
#ifdef _WIN32
                void* native_handle;
                void* fiber_from;
#else
                native_handle_t* native_handle;
#endif
                Executor* exec;
                void (*deleter)(Executor*);
                std::exception_ptr exception;
                std::atomic_bool end_of_function = false;
                std::atomic_bool first_time = false;
                std::atomic_flag run;
                control_flag flag;
            };
        }  // namespace light
    }      // namespace async
}  // namespace utils
