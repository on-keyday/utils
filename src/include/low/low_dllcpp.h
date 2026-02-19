/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <platform/windows/dllexport.h>
#if defined(FUTILS_AS_DLL)
#define low_DLL_EXPORT(Type) __stdcall __declspec(dllexport) Type
#define low_CLASS_EXPORT __declspec(dllexport)
#else
#define low_DLL_EXPORT(Type) Type
#define low_CLASS_EXPORT
#endif
