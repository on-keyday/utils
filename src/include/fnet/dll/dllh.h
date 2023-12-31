/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <platform/windows/dllexport.h>
#ifndef fnet_dll_export
#if defined(UTILS_AS_DLL)
#define fnet_dll_export(...) __declspec(dllimport) __VA_ARGS__ __stdcall
#define fnet_class_export __declspec(dllimport)
#else
#define fnet_dll_export(...) __VA_ARGS__
#define fnet_class_export
#endif
#endif
