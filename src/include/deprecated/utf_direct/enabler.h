/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// enabler
#pragma once
#include <type_traits>

namespace utils::utf {
    template <class T, size_t v>
    concept UTFSize = (sizeof(T) == v);
}  // namespace utils::utf
