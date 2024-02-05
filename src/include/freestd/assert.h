/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <freestd/freestd_macro.h>
#include <freestd/stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
void futils_assert_fail(const char* expr, const char* file, int line, const char* func);
#define assert(x) (!!(x) ? (void)0 : futils_assert_fail(#x, __FILE__, __LINE__, __func__))
#ifdef __cplusplus
}
#endif
