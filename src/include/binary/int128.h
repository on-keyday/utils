/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

namespace futils::binary {
#if defined(__SIZEOF_INT128__)
    using int128_t = __int128_t;
    using uint128_t = __uint128_t;
#define FUTILS_BINARY_SUPPORT_INT128 1
#else
    using int128_t = void;   // not supported
    using uint128_t = void;  // not supported
#endif
}  // namespace futils::binary
