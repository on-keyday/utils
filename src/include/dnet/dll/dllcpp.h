/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#ifdef _WIN32
#define dnet_dll_export(type) __declspec(dllexport) type __stdcall
#define dnet_dll_implement(type) type __stdcall
#define dnet_class_export __declspec(dllexport)
#else
#define dnet_dll_export(type) type
#define dnet_dll_implement(type) type
#define dnet_class_export
#endif