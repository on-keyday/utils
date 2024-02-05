/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <freestd/freestd_macro.h>
#include <freestd/stddef.h>
#include <freestd/stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

inline void *FUTILS_FREESTD_STDC(memset)(void *buf, char c, size_t n) {
    uint8_t *p = (uint8_t *)buf;
    while (n--)
        *p++ = c;
    return buf;
}

inline void *FUTILS_FREESTD_STDC(memcpy)(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--)
        *d++ = *s++;
    return dst;
}

inline size_t FUTILS_FREESTD_STDC(strlen)(const char *s) {
    size_t n = 0;
    while (*s++)
        n++;
    return n;
}

inline int FUTILS_FREESTD_STDC(strcmp)(const char *s1, const char *s2) {
    while (*s1 && *s2 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

#ifdef __cplusplus
namespace futils::freestd {
    using ::FUTILS_FREESTD_STDC(memcpy);
    using ::FUTILS_FREESTD_STDC(memset);
    using ::FUTILS_FREESTD_STDC(strcmp);
    using ::FUTILS_FREESTD_STDC(strlen);
}  // namespace futils::freestd
}
#endif
