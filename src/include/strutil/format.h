/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <type_traits>
#include <view/iovec.h>
#include <number/char_range.h>

namespace futils::strutil {
    enum class FormatError {
        none,
        invalid_format_string,
        missing_specifier,
        too_large,
    };

    struct FormatStr {
       private:
        const char* str;

       public:
        constexpr FormatStr(const char* str)
            : str(str) {}

        constexpr FormatError parse(auto&& callback, bool ignore_missing = false) const {
            if (!str) {
                if (std::is_constant_evaluated()) {
                    throw "FormatStr::parse: str is null";
                }
                return FormatError::invalid_format_string;
            }
            view::basic_rvec<char> range(str);
            const char* prev = range.begin();
            for (auto i = range.begin(); i != range.end(); i++) {
                if (*i == '%') {
                    callback(view::basic_rvec<char>(prev, i), 0);
                    prev = i;
                    i++;
                    if (i == range.end()) {
                        if (ignore_missing) {
                            return FormatError::none;
                        }
                        if (std::is_constant_evaluated()) {
                            throw "FormatStr::parse: invalid format string";
                        }
                        return FormatError::invalid_format_string;
                    }
                    if (*i == '%') {
                        callback(view::basic_rvec<char>("%"), 0);
                        prev = i + 1;
                        continue;
                    }
                    while (i != range.end() && !number::is_alpha(*i)) {
                        i++;
                    }
                    if (i == range.end()) {
                        if (ignore_missing) {
                            return FormatError::none;
                        }
                        if (std::is_constant_evaluated()) {
                            throw "FormatStr::parse: invalid format string";
                        }
                        return FormatError::missing_specifier;
                    }
                    FormatError err = callback(view::basic_rvec<char>(prev, i + 1), *i);
                    if (err != FormatError::none) {
                        return err;
                    }
                    prev = i + 1;
                }
            }
            callback(view::basic_rvec<char>(prev, range.end()), 0);
            return FormatError::none;
        }
    };

    struct ConstFormatStr {
        FormatStr fmt;
        consteval ConstFormatStr(const char* str)
            : fmt(str) {
            fmt.parse([](auto&&, auto&&) {
                return FormatError::none;
            });
        }
    };

    namespace test {
        constexpr auto format() {
            constexpr auto s = ConstFormatStr("hello %s");
            auto err = s.fmt.parse([](auto&& range, auto&& c) {
                if (c != 0) {
                    if (range[0] != '%' || range.back() != c) {
                        return FormatError::invalid_format_string;
                    }
                }
                return FormatError::none;
            });
            return err == FormatError::none;
        }

        static_assert(format());
    }  // namespace test

}  // namespace futils::strutil
