/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// vector - wrap default vector template
#pragma once

#if !defined(FUTILS_WRAP_VECTOR_TEMPLATE)
#include <vector>
#define FUTILS_WRAP_VECTOR_TEMPLATE std::vector
#endif

namespace futils {
    namespace wrap {
        template <class T>
        using vector = FUTILS_WRAP_VECTOR_TEMPLATE<T>;
    }
}  // namespace futils
