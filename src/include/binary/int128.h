/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

namespace utils::binary {
#if defined(__GCC__) || defined(__clang__)
    using int128_t = __int128_t;
    using uint128_t = __uint128_t;
#define UTILS_BINARY_SUPPORT_INT128 1
#else
    using int128_t = void;   // not supported
    using uint128_t = void;  // not supported
#endif
}  // namespace utils::binary
