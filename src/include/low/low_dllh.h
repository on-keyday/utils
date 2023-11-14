/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <platform/windows/dllexport.h>
#if defined(UTILS_AS_DLL)
#ifndef low_DLL_EXPORT
#define low_DLL_EXPORT(Type) __stdcall __declspec(dllimport) Type
#define low_CLASS_EXPORT __declspec(dllimport)
#endif
#else
#define low_DLL_EXPORT(Type) Type
#define low_CLASS_EXPORT
#endif
