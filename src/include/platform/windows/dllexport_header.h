/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// dllexport_header - dllexport included by header
#pragma once
#include "dllexport.h"
#if defined(_WIN32) && defined(UTILS_AS_DLL)
#ifndef DLL
#define DLL __declspec(dllimport)
#endif
#ifndef STDCALL
#define STDCALL __stdcall
#endif
#else
#undef DLL
#undef STDCALL
#define DLL
#define STDCALL
#endif
