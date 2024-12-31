/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <platform/windows/dllexport.h>
#if defined(FUTILS_AS_DLL)
#define fnetserv_dll_export(type) __declspec(dllexport) type __stdcall
#define fnetserv_dll_internal(type) type __stdcall
#define fnetserv_class_export __declspec(dllexport)
#else
#define fnetserv_dll_export(type) type
#define fnetserv_dll_internal(type) type
#define fnetserv_class_export
#endif
