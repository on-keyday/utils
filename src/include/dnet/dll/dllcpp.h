/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#ifdef _WIN32
#define dll_export(type) __declspec(dllexport) type __stdcall
#define dll_internal(type) type __stdcall
#define class_export __declspec(dllexport)
#else
#define dll_export(type) type
#define dll_internal(type) type
#define class_export
#endif