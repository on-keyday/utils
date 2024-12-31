/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <freestd/stdlib.h>

FUTILS_FREESTD_STUB_WEAK
extern "C" void FUTILS_FREESTD_STDC(exit)(int status) {
    for (;;) {
        __builtin_unreachable();
    }
}

FUTILS_FREESTD_STUB_WEAK
extern "C" void FUTILS_FREESTD_STDC(abort)(void) {
    FUTILS_FREESTD_STDC(exit)
    (1);
}