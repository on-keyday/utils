/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#if __has_include(<futils_platform_stdint.h>)
#include <futils_platform_stdint.h>
#else
#define FUTILS_FREESTD_STDINT_UINT8_T unsigned char
#define FUTILS_FREESTD_STDINT_UINT16_T unsigned short
#define FUTILS_FREESTD_STDINT_UINT32_T unsigned int
#define FUTILS_FREESTD_STDINT_UINT64_T unsigned long long
#define FUTILS_FREESTD_STDINT_INT8_T char
#define FUTILS_FREESTD_STDINT_INT16_T short
#define FUTILS_FREESTD_STDINT_INT32_T int
#define FUTILS_FREESTD_STDINT_INT64_T long long
#endif

typedef FUTILS_FREESTD_STDINT_UINT8_T uint8_t;
typedef FUTILS_FREESTD_STDINT_UINT16_T uint16_t;
typedef FUTILS_FREESTD_STDINT_UINT32_T uint32_t;
typedef FUTILS_FREESTD_STDINT_UINT64_T uint64_t;
typedef FUTILS_FREESTD_STDINT_INT8_T int8_t;
typedef FUTILS_FREESTD_STDINT_INT16_T int16_t;
typedef FUTILS_FREESTD_STDINT_INT32_T int32_t;
typedef FUTILS_FREESTD_STDINT_INT64_T int64_t;

#if !defined(__SIZEOF_POINTER__)
#error "TOO OLD COMPILER"
#endif

#if __SIZEOF_POINTER__ == 8
typedef uint64_t uintptr_t;
typedef int64_t intptr_t;
#elif __SIZEOF_POINTER__ == 4
typedef uint32_t uintptr_t;
typedef int32_t intptr_t;
#else
#error "TOO OLD COMPILER"
#endif
