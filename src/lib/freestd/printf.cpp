/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "printf_internal.h"
#include <freestd/stdio.h>

extern "C" int FUTILS_FREESTD_STDC(vprintf)(const char* format, va_list args) {
    struct {
        void push_back(futils::byte c) {
            FUTILS_FREESTD_STDC(putchar)
            (c);
        }
    } pb;
    return vprintf_internal(pb, format, args);
}

extern "C" int FUTILS_FREESTD_STDC(puts)(const char* str) {
    auto range = futils::view::basic_rvec<char>(str);
    for (auto c : range) {
        FUTILS_FREESTD_STDC(putchar)
        (c);
    }
    return range.size();
}
