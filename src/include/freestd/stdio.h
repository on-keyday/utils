/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <freestd/stdarg.h>
#include <freestd/stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
int printf(const char* format, ...);
int vprintf(const char* format, va_list args);
int snprintf(char* str, size_t size, const char* format, ...);
int vsnprintf(char* str, size_t size, const char* format, va_list args);
int putchar(int c);
int getchar();
int puts(const char* str);

// kernel dependent
// if retry <= 0, this function will block until a character is received
// if otherwise, this function will retry retry-1 times and return -1 if no character is received
int getchar_nonblock(int retry);  // this is maybe slow

#ifdef __cplusplus
namespace futils::freestd {
    using ::printf;
    using ::putchar;
}  // namespace futils::freestd
}
#endif
