/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <freestd/stdio.h>
#include <freestd/stdlib.h>

extern "C" void FUTILS_FREESTD_STDC(exit)(int status) {
    for (;;) {
        __builtin_unreachable();
    }
}

extern "C" void FUTILS_FREESTD_STDC(abort)(void) {
    FUTILS_FREESTD_STDC(exit)
    (1);
}

extern "C" int FUTILS_FREESTD_STDC(putchar)(int c) {
    // stub ignore
    return c;
}
