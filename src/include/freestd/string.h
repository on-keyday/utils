/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <freestd/stddef.h>
#include <freestd/stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

inline void *memset(void *buf, char c, size_t n) {
    uint8_t *p = (uint8_t *)buf;
    while (n--)
        *p++ = c;
    return buf;
}

#ifdef __cplusplus
namespace futils::freestd {
    using ::memset;
}
}
#endif
