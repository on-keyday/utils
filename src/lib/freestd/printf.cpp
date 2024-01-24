/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <freestd/stdarg.h>
#include <freestd/stdio.h>
#include <strutil/format.h>
#include <number/to_string.h>
#include <console/ansiesc.h>

extern "C" int printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    int count = vprintf(format, args);
    va_end(args);
    return count;
}

extern "C" int vprintf(const char* format, va_list args) {
    futils::strutil::FormatStr fmt(format);
    int count = 0;
    fmt.parse(
        [&](futils::view::basic_rvec<char> range, int c) {
            if (c == 0) {
                for (auto c : range) {
                    putchar(futils::byte(c));
                }
                count += range.size();
                return futils::strutil::FormatError::none;
            }
            switch (c) {
                case 's': {
                    auto s = va_arg(args, const char*);
                    auto txt = futils::view::basic_rvec<char>(s);
                    for (auto c : txt) {
                        putchar(futils::byte(c));
                    }
                    count += txt.size();
                    break;
                }
                case 'c': {
                    putchar(futils::byte(va_arg(args, int)));
                    count++;
                    break;
                }
                case 'p': {
                    auto p = va_arg(args, void*);
                    struct {
                        int* count;
                        void push_back(char c) {
                            putchar(c);
                            (*count)++;
                        }
                    } cout{&count};
                    futils::number::to_string(cout, (uintptr_t)p, 16);
                    break;
                }
                case 'd':
                case 'x': {
                    auto i = va_arg(args, int);
                    struct {
                        int* count;
                        void push_back(char c) {
                            putchar(c);
                            (*count)++;
                        }
                    } cout{&count};
                    if (c == 'x') {
                        futils::number::to_string(cout, (unsigned int)i, 16);
                    }
                    else {
                        futils::number::to_string(cout, i, 10);
                    }
                    break;
                }
            }
            return futils::strutil::FormatError::none;
        },
        true);
    return count;
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
#ifdef __riscv
        __asm__ volatile("wfi");
#endif
        // no return
    }
}
