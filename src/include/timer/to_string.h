/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
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
#include <strutil/format.h>

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

    template <class Out>
    constexpr auto to_string_v(Out& o, strutil::FormatStr fmt, const Time& t, const TimeOrigin& origin = unix_epoch, char insert = '0') {
        auto t2 = origin.from_time(t);
        return fmt.parse([&](view::basic_rvec<char> range, char t) {
            switch (t) {
                case 0:
                    strutil::append(o, range);
                    break;
                case 'Y':
                    if (insert) {
                        number::insert_space(o, 4, t2.year, 10, insert);
                    }
                    number::to_string(o, t2.year);
                    break;
                case 'M':
                    if (insert) {
                        number::insert_space(o, 2, t2.month, 10, insert);
                    }
                    number::to_string(o, t2.month);
                    break;
                case 'D':
                    if (insert) {
                        number::insert_space(o, 2, t2.day, 10, insert);
                    }
                    number::to_string(o, t2.day);
                    break;
                case 'h':
                    if (insert) {
                        number::insert_space(o, 2, t2.hour, 10, insert);
                    }
                    number::to_string(o, t2.hour);
                    break;
                case 'm':
                    if (insert) {
                        number::insert_space(o, 2, t2.minute, 10, insert);
                    }
                    number::to_string(o, t2.minute);
                    break;
                case 's':
                    if (insert) {
                        number::insert_space(o, 2, t2.second, 10, insert);
                    }
                    number::to_string(o, t2.second);
                    break;
                case 'f':
                    if (insert) {
                        number::insert_space(o, 3, t2.nanosecond / nano_per_milli, 10, insert);
                    }
                    number::to_string(o, t2.nanosecond / nano_per_milli);
                    break;
                case 'u':
                    if (insert) {
                        number::insert_space(o, 6, t2.nanosecond / nano_per_micro, 10, insert);
                    }
                    number::to_string(o, t2.nanosecond / nano_per_micro);
                    break;
                case 'n':
                    if (insert) {
                        number::insert_space(o, 9, t2.nanosecond, 10, insert);
                    }
                    number::to_string(o, t2.nanosecond);
                    break;
                default:
                    strutil::append(o, "<unknown>");
                    break;
            }
            return strutil::FormatError::none;
        });
    }

    template <class Out>
    constexpr auto to_string(Out& o, strutil::ConstFormatStr fmt, const Time& t, const TimeOrigin& origin = unix_epoch) {
        return to_string_v(o, fmt.fmt, t, origin);
    }

    template <class Out>
    constexpr Out to_string(strutil::ConstFormatStr fmt, const Time& t, const TimeOrigin& origin = unix_epoch) {
        Out o{};
        to_string(o, fmt, t, origin);
        return o;
    }

    template <class Out>
    constexpr Out to_string_v(strutil::FormatStr fmt, const Time& t, const TimeOrigin& origin = unix_epoch) {
        Out o{};
        to_string_v(o, fmt, t, origin);
        return o;
    }

    namespace test {
        constexpr auto format_str = strutil::ConstFormatStr("%Y-%M-%D %h:%m:%s.%n");
        constexpr auto nengappi_str = strutil::ConstFormatStr("%Y年%M月%D日 %h時%m分%s秒");

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
