/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// detect - detect coroutine headers
#pragma once

#ifdef __cpp_coroutines
#if !defined(_WIN32) || !defined(__clang__)
#if __has_include(<coroutine>)
#include <coroutine>
#define UTILS_COROUTINE_NAMESPACE std
#elif __has_include(<experimental/coroutine>)
#include <experimental/coroutine>
#define UTILS_COROUTINE_NAMESPACE std::experimental
#endif
#endif
#endif
#ifdef UTILS_COROUTINE_NAMESPACE
namespace utils {
    namespace async {
        namespace coro_ns = UTILS_COROUTINE_NAMESPACE;
    }
}  // namespace utils
#endif
