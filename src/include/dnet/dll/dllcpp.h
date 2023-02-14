/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#ifdef _WIN32
#define dnet_dll_export(...) __declspec(dllexport) __VA_ARGS__ __stdcall
#define dnet_dll_implement(...) __VA_ARGS__ __stdcall
#define dnet_class_export __declspec(dllexport)
#else
#define dnet_dll_export(...) __VA_ARGS__
#define dnet_dll_implement(...) __VA_ARGS__
#define dnet_class_export
#endif
