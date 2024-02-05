/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#if __has_include(<futils_platform_stddef>)
#include <futils_platform_stddef.h>
#else
#if !defined(__SIZEOF_SIZE_T__)
#error "TOO OLD COMPILER"
#endif
#if __SIZEOF_SIZE_T__ == 8
#define FUTILS_FREESTD_STDDEF_SIZE_T unsigned long long
#define FUTILS_FREESTD_STDDEF_PTRDIFF_T long long
#elif __SIZEOF_SIZE_T__ == 4
#define FUTILS_FREESTD_STDDEF_SIZE_T unsigned int
#define FUTILS_FREESTD_STDDEF_PTRDIFF_T int
#else
#error "TOO OLD COMPILER"
#endif
#endif

typedef FUTILS_FREESTD_STDDEF_SIZE_T size_t;
typedef FUTILS_FREESTD_STDDEF_PTRDIFF_T ptrdiff_t;
