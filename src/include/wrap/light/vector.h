/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// vector - wrap default vector template
#pragma once

#if !defined(UTILS_WRAP_VECTOR_TEMPLATE)
#include <vector>
#define UTILS_WRAP_VECTOR_TEMPLATE std::vector
#endif

namespace utils {
    namespace wrap {
        template <class T>
        using vector = UTILS_WRAP_VECTOR_TEMPLATE<T>;
    }
}  // namespace utils
