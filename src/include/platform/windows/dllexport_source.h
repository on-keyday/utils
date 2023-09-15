/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// dllexport_source - dllexport included by source
#pragma once
#include "dllexport.h"
#if defined(UTILS_AS_DLL)
#define utils_DLL_EXPORT __declspec(dllexport)
#else
#define utils_DLL_EXPORT
#endif
