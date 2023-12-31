/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// dllexport_header - dllexport included by header
#pragma once
#include "dllexport.h"
#if defined(UTILS_AS_DLL)
#ifndef utils_DLL_EXPORT
#define utils_DLL_EXPORT __declspec(dllimport)
#endif
#ifndef STDCALL
#define STDCALL __stdcall
#endif
#else
#undef utils_DLL_EXPORT
#undef STDCALL
#define utils_DLL_EXPORT
#define STDCALL
#endif
