/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <platform/windows/dllexport.h>
#ifndef coro_DLL_EXPORT
#if defined(FUTILS_AS_DLL)
#define coro_DLL_EXPORT(Type) __declspec(dllexport) Type
#else
#define coro_DLL_EXPORT(Type) Type
#endif
#endif
