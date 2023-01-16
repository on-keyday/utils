/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#pragma once

#define LIBQUIC_DLL_EXPORT
#ifdef _WIN32
#define dll(TYPE) TYPE __stdcall
#define Dll(TYPE) __declspec(dllexport) TYPE __stdcall
#define CDll(TYPE) extern "C" Dll(TYPE)
#else
#define dll(TYPE) TYPE
#define Dll(TYPE) TYPE
#define CDll(TYPE) TYPE
#endif
