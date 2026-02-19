/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <platform/detect.h>

#ifdef __FUTILS_FREESTD_NOT_STDC__
#define FUTILS_FREESTD_STDC(name) futils_##name
#else
#define FUTILS_FREESTD_STDC(name) name
#endif

#if !defined(__FUTILS_FREESTD_NOT_STD_CPP__) && defined(FUTILS_PLATFORM_FREESTANDING)
#define FUTILS_FREESTD_STD_CPP
#endif

#define FUTILS_FREESTD_STUB_WEAK __attribute__((weak))
