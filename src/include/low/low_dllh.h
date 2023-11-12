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
