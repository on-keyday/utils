/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// dllexport_header - dllexport included by header
#pragma once
#include "dllexport.h"
#if defined(FUTILS_AS_DLL)
#ifndef futils_DLL_EXPORT
#define futils_DLL_EXPORT __declspec(dllimport)
#endif
#ifndef STDCALL
#define STDCALL __stdcall
#endif
#else
#undef futils_DLL_EXPORT
#undef STDCALL
#define futils_DLL_EXPORT
#define STDCALL
#endif
