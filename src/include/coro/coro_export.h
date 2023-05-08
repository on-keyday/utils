/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#ifndef coro_DLL_EXPORT
#ifdef _WIN32
#define coro_DLL_EXPORT(Type) __declspec(dllimport) Type
#else
#define coro_DLL_EXPORT(Type) Type
#endif
#endif
