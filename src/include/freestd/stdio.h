/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <freestd/stdarg.h>
#ifdef __cplusplus
extern "C" {
#endif
int printf(const char* format, ...);
int vprintf(const char* format, va_list args);
void putchar(int c);

#ifdef __cplusplus
namespace futils::freestd {
    using ::printf;
    using ::putchar;
}  // namespace futils::freestd
}
#endif
