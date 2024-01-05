/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "time.h"
#include <view/iovec.h>
#include <number/char_range.h>
#include <strutil/append.h>
#include <number/to_string.h>
#include <strutil/equal.h>
#include <number/insert_space.h>

namespace futils::timer {

    enum class FormatType {
        text,
        year,
        month,
        day,
        hour,
        minute,
        second,
        millisecond,
        microsecond,
        nanosecond,
        unknown,
    };

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

        constexpr FormatError parse(auto&& callback) {
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
                    callback(view::basic_rvec<char>(prev, i), FormatType::text);
                    prev = i;
                    i++;
                    if (i == range.end()) {
                        if (std::is_constant_evaluated()) {
                            throw "FormatStr::parse: invalid format string";
                        }
                        return FormatError::invalid_format_string;
                    }
                    if (*i == '%') {
                        callback(view::basic_rvec<char>("%"), FormatType::text);
                        prev = i;
                        continue;
                    }
                    while (i != range.end() && !number::is_alpha(*i)) {
                        i++;
                    }
                    if (i == range.end()) {
                        if (std::is_constant_evaluated()) {
                            throw "FormatStr::parse: invalid format string";
                        }
                        return FormatError::missing_specifier;
                    }
                    FormatError err = FormatError::none;
                    switch (*i) {
                        case 'Y':
                            err = callback(view::basic_rvec<char>(prev, i),
                                           FormatType::year);
                            break;
                        case 'M':
                            err = callback(view::basic_rvec<char>(prev, i),
                                           FormatType::month);
                            break;
                        case 'D':
                            err = callback(view::basic_rvec<char>(prev, i),
                                           FormatType::day);
                            break;
                        case 'h':
                            err = callback(view::basic_rvec<char>(prev, i),
                                           FormatType::hour);
                            break;
                        case 'm':
                            err = callback(view::basic_rvec<char>(prev, i),
                                           FormatType::minute);
                            break;
                        case 's':
                            err = callback(view::basic_rvec<char>(prev, i),
                                           FormatType::second);
                            break;
                        case 'f':
                            err = callback(view::basic_rvec<char>(prev, i),
                                           FormatType::millisecond);
                            break;
                        case 'u':
                            err = callback(view::basic_rvec<char>(prev, i),
                                           FormatType::microsecond);
                            break;
                        case 'n':
                            err = callback(view::basic_rvec<char>(prev, i),
                                           FormatType::nanosecond);
                            break;
                        default:
                            err = callback(view::basic_rvec<char>(prev, i),
                                           FormatType::unknown);
                            break;
                    }
                    prev = i + 1;
                }
            }
            callback(view::basic_rvec<char>(prev, range.end()), FormatType::text);
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

    template <class Out>
    constexpr auto to_string_v(Out& o, FormatStr fmt, const Time& t, const TimeOrigin& origin = unix_epoch, char insert = '0') {
        auto t2 = origin.from_time(t);
        return fmt.parse([&](view::basic_rvec<char> range, FormatType t) {
            switch (t) {
                case FormatType::text:
                    strutil::append(o, range);
                    break;
                case FormatType::year:
                    if (insert) {
                        number::insert_space(o, 4, t2.year, 10, insert);
                    }
                    number::to_string(o, t2.year);
                    break;
                case FormatType::month:
                    if (insert) {
                        number::insert_space(o, 2, t2.month, 10, insert);
                    }
                    number::to_string(o, t2.month);
                    break;
                case FormatType::day:
                    if (insert) {
                        number::insert_space(o, 2, t2.day, 10, insert);
                    }
                    number::to_string(o, t2.day);
                    break;
                case FormatType::hour:
                    if (insert) {
                        number::insert_space(o, 2, t2.hour, 10, insert);
                    }
                    number::to_string(o, t2.hour);
                    break;
                case FormatType::minute:
                    if (insert) {
                        number::insert_space(o, 2, t2.minute, 10, insert);
                    }
                    number::to_string(o, t2.minute);
                    break;
                case FormatType::second:
                    if (insert) {
                        number::insert_space(o, 2, t2.second, 10, insert);
                    }
                    number::to_string(o, t2.second);
                    break;
                case FormatType::millisecond:
                    if (insert) {
                        number::insert_space(o, 3, t2.nanosecond / nano_per_milli, 10, insert);
                    }
                    number::to_string(o, t2.nanosecond / nano_per_milli);
                    break;
                case FormatType::microsecond:
                    if (insert) {
                        number::insert_space(o, 6, t2.nanosecond / nano_per_micro, 10, insert);
                    }
                    number::to_string(o, t2.nanosecond / nano_per_micro);
                    break;
                case FormatType::nanosecond:
                    if (insert) {
                        number::insert_space(o, 9, t2.nanosecond, 10, insert);
                    }
                    number::to_string(o, t2.nanosecond);
                    break;
                case FormatType::unknown:
                    strutil::append(o, "<unknown>");
                    break;
            }
            return FormatError::none;
        });
    }

    template <class Out>
    constexpr auto to_string(Out& o, ConstFormatStr fmt, const Time& t, const TimeOrigin& origin = unix_epoch) {
        return to_string_v(o, fmt.fmt, t, origin);
    }

    template <class Out>
    constexpr Out to_string(ConstFormatStr fmt, const Time& t, const TimeOrigin& origin = unix_epoch) {
        Out o{};
        to_string(o, fmt, t, origin);
        return o;
    }

    template <class Out>
    constexpr Out to_string_v(FormatStr fmt, const Time& t, const TimeOrigin& origin = unix_epoch) {
        Out o{};
        to_string_v(o, fmt, t, origin);
        return o;
    }

    namespace test {
        constexpr auto format_str = ConstFormatStr("%Y-%M-%D %h:%m:%s.%n");
        constexpr auto nengappi_str = ConstFormatStr("%Y年%M月%D日 %h時%m分%s秒");

        constexpr auto test_format() {
            Time t = unix_epoch.to_time(unix_epoch);
            number::Array<char, 100, true> o{};
            to_string(o, format_str, t);
            if (!strutil::equal(o, "1970-01-01 00:00:00.000000000")) {
                return false;
            }
            o.i = 0;
            to_string(o, nengappi_str, t);
            if (!strutil::equal(o, "1970年01月01日 00時00分00秒")) {
                return false;
            }
            // now, 2024-01-02 17:23:50.123456789
            auto t2 = unix_epoch.to_time({2024, 1, 2, 17, 23, 50, 123456789});
            o.i = 0;
            to_string(o, format_str, t2);
            if (!strutil::equal(o, "2024-01-02 17:23:50.123456789")) {
                return false;
            }
            o.i = 0;
            to_string(o, nengappi_str, t2);
            if (!strutil::equal(o, "2024年01月02日 17時23分50秒")) {
                return false;
            }
            return true;
        }

        static_assert(test_format());
    }  // namespace test

}  // namespace futils::timer
