/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// set - wrap default set template
#pragma once

#if !defined(FUTILS_WRAP_SET_TEMPLATE)
#include <set>
#define FUTILS_WRAP_SET_TEMPLATE std::set
#endif

namespace futils {
    namespace wrap {
        template <class T>
        using set = FUTILS_WRAP_SET_TEMPLATE<T>;
    }
}  // namespace futils
