/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <freestd/stdarg.h>
#include <freestd/stdio.h>
#include <freestd/stdlib.h>
#include <strutil/format.h>
#include <number/to_string.h>
#include <console/ansiesc.h>
#include <helper/pushbacker.h>

int vprintf_internal(auto&& pb, const char* format, va_list args) {
    futils::strutil::FormatStr fmt(format);
    futils::helper::CountPushBacker<decltype(pb)> pb_count{pb};
    fmt.parse(
        [&](futils::view::basic_rvec<char> range, int c) {
            if (c == 0) {
                for (auto c : range) {
                    pb_count.push_back(futils::byte(c));
                }
                return futils::strutil::FormatError::none;
            }
            switch (c) {
                case 's': {
                    auto s = va_arg(args, const char*);
                    auto txt = futils::view::basic_rvec<char>(s);
                    for (auto c : txt) {
                        pb_count.push_back(futils::byte(c));
                    }
                    break;
                }
                case 'c': {
                    auto c = va_arg(args, int);
                    pb_count.push_back(futils::byte(c));
                    break;
                }
                case 'p': {
                    auto p = va_arg(args, void*);
                    futils::number::to_string(pb_count, (uintptr_t)p, 16);
                    break;
                }
                case 'd':
                case 'x': {
                    auto i = va_arg(args, int);
                    if (c == 'x') {
                        futils::number::to_string(pb_count, (unsigned int)i, 16);
                    }
                    else {
                        futils::number::to_string(pb_count, i, 10);
                    }
                    break;
                }
            }
            return futils::strutil::FormatError::none;
        },
        true);
    return pb_count.count;
}

extern "C" int vprintf(const char* format, va_list args) {
    struct {
        void push_back(futils::byte c) {
            putchar(c);
        }
    } pb;
    return vprintf_internal(pb, format, args);
}

extern "C" int printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    int count = vprintf(format, args);
    va_end(args);
    return count;
}

extern "C" int snprintf(char* str, size_t size, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int count = vsnprintf(str, size, format, args);
    va_end(args);
    return count;
}

extern "C" int vsnprintf(char* str, size_t size, const char* format, va_list args) {
    if (size == 0) {
        return 0;
    }
    futils::helper::CharVecPushbacker pb{str, 0, size - 1};
    auto ret = vprintf_internal(pb, format, args);
    pb.text[pb.size()] = 0;
    return ret;
}

extern "C" void futils_kernel_panic(const char* file, int line, const char* fmt, ...) {
    auto red = futils::console::escape::letter_color<futils::console::escape::ColorPalette::red>.c_str();
    auto reset = futils::console::escape::color_reset.c_str();
    printf("%sPANIC%s: %s:%d: ", red, reset, file, line);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    for (;;) {
        exit(1);
        // no return
    }
}
