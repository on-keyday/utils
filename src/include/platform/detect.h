/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#ifndef UTILS_PLATFORM
#if defined(_WIN32)
#define UTILS_PLATFORM_WINDOWS
#elif defined(__wasi__)
#define UTILS_PLATFORM_WASI
#elif defined(__unix__)
#define UTILS_PLATFORM_UNIX
#if defined(__linux__)
#define UTILS_PLATFORM_LINUX
#endif
#if defined(__EMSCRIPTEN__)
#define UTILS_PLATFORM_EMSCRIPTEN
#endif
#if defined(__ANDROID__)
#define UTILS_PLATFORM_ANDROID
#endif
#if defined(__APPLE__) && defined(__MACH__)
#define UTILS_PLATFORM_MACOS
#endif
#else
#error "Unsupported platform"
#endif
#define UTILS_PLATFORM 1
#endif
