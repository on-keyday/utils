/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#ifndef FUTILS_PLATFORM
#if defined(_WIN32)
#define FUTILS_PLATFORM_WINDOWS
#elif defined(__APPLE__) && defined(__MACH__)
#define FUTILS_PLATFORM_MACOS
#elif defined(__wasi__)
#define FUTILS_PLATFORM_WASI
#elif defined(__unix__)
#define FUTILS_PLATFORM_UNIX
#if defined(__linux__)
#define FUTILS_PLATFORM_LINUX
#endif
#if defined(__EMSCRIPTEN__)
#define FUTILS_PLATFORM_EMSCRIPTEN
#endif
#if defined(__ANDROID__)
#define FUTILS_PLATFORM_ANDROID
#endif
#else
#error "Unsupported platform"
#endif
#define FUTILS_PLATFORM 1
#endif
