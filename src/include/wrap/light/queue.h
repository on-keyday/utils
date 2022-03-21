/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// queue - wrap default queue template
#pragma once

#if !defined(UTILS_WRAP_QUEUE_TEMPLATE)
#include <deque>
#define UTILS_WRAP_QUEUE_TEMPLATE std::deque
#endif

namespace utils {
    namespace wrap {
        template <class T>
        using queue = UTILS_WRAP_QUEUE_TEMPLATE<T>;
    }
}  // namespace utils
