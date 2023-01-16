/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// async macro - async await macro
#pragma once

namespace utils {
    namespace async {
#define AWAIT_CTX(CTX, EXPR) EXPR.wait_until(CTX).get()
#define AWAIT(EXPR) AWAIT_CTX(ctx, EXPR)
    }  // namespace async
}  // namespace utils
