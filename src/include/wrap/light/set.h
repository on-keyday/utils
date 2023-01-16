/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// set - wrap default set template
#pragma once

#if !defined(UTILS_WRAP_SET_TEMPLATE)
#include <set>
#define UTILS_WRAP_SET_TEMPLATE std::set
#endif

namespace utils {
    namespace wrap {
        template <class T>
        using set = UTILS_WRAP_SET_TEMPLATE<T>;
    }
}  // namespace utils
