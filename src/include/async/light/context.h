/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// context - light context
#pragma once

namespace utils {
    namespace async {
        namespace light {
            struct Context;
#ifdef _WIN32
#define ASYNC_NO_VTABLE_ __declspec(novtable)
#else
#define ASYNC_NO_VTABLE_
#endif
            struct ASYNC_NO_VTABLE_ Executor {
                virtual void execute(Context) = 0;
                virtual ~Executor() {}
            };

            struct Context {
            };
        }  // namespace light
    }      // namespace async
}  // namespace utils
