/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <platform/windows/dllexport.h>
#ifndef fnetserv_dll_export
#if defined(UTILS_AS_DLL)
#define fnetserv_dll_export(type) __declspec(dllimport) type __stdcall
#define fnetserv_class_export __declspec(dllimport)
#else
#define fnetserv_dll_export(type) type
#define fnetserv_class_export
#endif
#endif
