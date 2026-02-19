
/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "printf_internal.h"
#include <freestd/stdio.h>
extern "C" int FUTILS_FREESTD_STDC(vsnprintf)(char* str, size_t size, const char* format, va_list args) {
    if (size == 0) {
        return 0;
    }
    futils::helper::CharVecPushbacker pb{str, 0, size - 1};
    auto ret = vprintf_internal(pb, format, args);
    pb.text[pb.size()] = 0;
    return ret;
}
