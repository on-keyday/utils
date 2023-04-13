/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// iterator_base - iterator base class
#pragma once
#include <iterator>

namespace utils {
    namespace helper {
        template <class C, class T, class D = std::ptrdiff_t, class P = T*, class R = T&>
        struct iterator_base {
            using difference_type = Diff;
            using value_type = T;
            using pointer = Ptr;
            using reference = Ref;
            using terator_category = Category;
        };
    }  // namespace helper
}  // namespace utils
