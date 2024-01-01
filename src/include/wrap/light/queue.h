/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// queue - wrap default queue template
#pragma once

#if !defined(FUTILS_WRAP_QUEUE_TEMPLATE)
#include <deque>
#define FUTILS_WRAP_QUEUE_TEMPLATE std::deque
#endif

namespace futils {
    namespace wrap {
        template <class T>
        using queue = FUTILS_WRAP_QUEUE_TEMPLATE<T>;
    }
}  // namespace futils
