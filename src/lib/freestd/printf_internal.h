/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <freestd/stdarg.h>
#include <strutil/format.h>
#include <number/to_string.h>
#include <helper/pushbacker.h>

constexpr int vprintf_internal(auto&& pb, const char* format, va_list args) {
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
