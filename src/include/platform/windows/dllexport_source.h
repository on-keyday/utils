/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// dllexport_source - dllexport included by source
#pragma once
#include "dllexport.h"
#if defined(FUTILS_AS_DLL)
#define futils_DLL_EXPORT __declspec(dllexport)
#else
#define futils_DLL_EXPORT
#endif
