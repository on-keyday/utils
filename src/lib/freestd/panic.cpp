/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <freestd/stdio.h>
#include <console/ansiesc.h>

extern "C" void futils_kernel_panic(const char* file, int line, const char* fmt, ...) {
    constexpr auto red = futils::console::escape::letter_color<futils::console::escape::ColorPalette::red>.c_str();
    constexpr auto reset = futils::console::escape::color_reset.c_str();
    FUTILS_FREESTD_STDC(printf)
    ("%sPANIC%s: %s:%d: ", red, reset, file, line);
    va_list args;
    va_start(args, fmt);
    FUTILS_FREESTD_STDC(vprintf)
    (fmt, args);
    va_end(args);
    for (;;) {
        FUTILS_FREESTD_STDC(exit)
        (1);
        // no return
    }
}

extern "C" void futils_assert_fail(const char* expr, const char* file, int line, const char* func) {
    constexpr auto red = futils::console::escape::letter_color<futils::console::escape::ColorPalette::red>.c_str();
    constexpr auto reset = futils::console::escape::color_reset.c_str();
    FUTILS_FREESTD_STDC(printf)
    ("%sASSERTION FAILED%s: %s:%d: %s: %s\n", red, reset, file, line, func, expr);
    for (;;) {
        FUTILS_FREESTD_STDC(exit)
        (1);
        // no return
    }
}
