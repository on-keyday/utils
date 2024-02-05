/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <freestd/freestd_macro.h>
#include <freestd/stdarg.h>
#include <freestd/stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
int FUTILS_FREESTD_STDC(printf)(const char* format, ...);
int FUTILS_FREESTD_STDC(vprintf)(const char* format, va_list args);
int FUTILS_FREESTD_STDC(snprintf)(char* str, size_t size, const char* format, ...);
int FUTILS_FREESTD_STDC(vsnprintf)(char* str, size_t size, const char* format, va_list args);
int FUTILS_FREESTD_STDC(putchar)(int c);
int FUTILS_FREESTD_STDC(getchar)();
int FUTILS_FREESTD_STDC(puts)(const char* str);

// kernel dependent
// if retry <= 0, this function will block until a character is received
// if otherwise, this function will retry retry-1 times and return -1 if no character is received
int FUTILS_FREESTD_STDC(getchar_nonblock)(int retry);  // this is maybe slow

#ifdef __cplusplus
namespace futils::freestd {
    using ::FUTILS_FREESTD_STDC(getchar);
    using ::FUTILS_FREESTD_STDC(getchar_nonblock);
    using ::FUTILS_FREESTD_STDC(printf);
    using ::FUTILS_FREESTD_STDC(putchar);
    using ::FUTILS_FREESTD_STDC(puts);
    using ::FUTILS_FREESTD_STDC(snprintf);
    using ::FUTILS_FREESTD_STDC(vprintf);
    using ::FUTILS_FREESTD_STDC(vsnprintf);
}  // namespace futils::freestd
}
#endif
