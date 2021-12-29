/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// date - http date parser
// almost copied from socklib
#pragma once

#include "../../core/sequencer.h"
#include "../../helper/splits.h"
#include "../../wrap/lite/enum.h"
#include <ctime>

namespace utils {
    namespace net {
        namespace date {
            enum class DayName : std::uint8_t {
                unset,
                sun,
                mon,
                tue,
                wed,
                thu,
                fri,
                sat,

                invalid = 0xff,
            };

            BEGIN_ENUM_STRING_MSG(DayName, get_dayname)
            ENUM_STRING_MSG(DayName::sun, "Sun")
            ENUM_STRING_MSG(DayName::mon, "Mon")
            ENUM_STRING_MSG(DayName::tue, "Tue")
            ENUM_STRING_MSG(DayName::wed, "Wed")
            ENUM_STRING_MSG(DayName::thu, "Thu")
            ENUM_STRING_MSG(DayName::fri, "Fri")
            ENUM_STRING_MSG(DayName::sat, "Sat")
            END_ENUM_STRING_MSG("")

            enum class Month : std::uint8_t {
                unset,
                jan,
                feb,
                mar,
                apr,
                may,
                jun,
                jul,
                aug,
                sep,
                oct,
                nov,
                dec,

                invalid = 0xff,
            };

            BEGIN_ENUM_STRING_MSG(Month, get_month)
            ENUM_STRING_MSG(Month::jan, "Jan")
            ENUM_STRING_MSG(Month::feb, "Feb")
            ENUM_STRING_MSG(Month::mar, "Mar")
            ENUM_STRING_MSG(Month::apr, "Apr")
            ENUM_STRING_MSG(Month::may, "May")
            ENUM_STRING_MSG(Month::jun, "Jun")
            ENUM_STRING_MSG(Month::jul, "Jul")
            ENUM_STRING_MSG(Month::aug, "Aug")
            ENUM_STRING_MSG(Month::sep, "Sep")
            ENUM_STRING_MSG(Month::oct, "Oct")
            ENUM_STRING_MSG(Month::nov, "Nov")
            ENUM_STRING_MSG(Month::dec, "Dec")
            END_ENUM_STRING_MSG("")

            struct Date {
                DayName dayname = DayName::unset;
                std::uint8_t day = 0;
                Month month = Month::unset;
                std::uint8_t hour = 0;
                std::uint8_t minute = 0;
                std::uint8_t second = 0;
                std::uint16_t year = 0;
            };

            constexpr Date get_invalid_date() {
                return Date{
                    .dayname = DayName::invalid,
                    .day = 0xff,
                    .month = Month::invalid,
                    .hour = 0xff,
                    .minute = 0xff,
                    .second = 0xff,
                    .year = 0xffff};
            }

            constexpr auto invalid_date = get_invalid_date();

            enum class DateError {
                none,
                not_date,
                no_dayname,
                not_dayname,
                no_day,
                not_day,
                not_month,
                no_year,
                not_year,
                no_time,
                no_hour,
                not_hour,
                no_minute,
                not_minute,
                no_second,
                not_second,
                not_GMT,
            };

            using DateErr = wrap::EnumWrap<DateError, DateError::none, DateError::not_date>;

            template <class String, template <class...> class Vec>
            struct DateParser {
                using string_t = String;

               private:
                template <class E, int max, class F>
                static bool get_expected(E& toset, string_t& src, F& f) {
                    auto seq = make_ref_seq(src);
                    for (auto i = 1; i <= max; i++) {
                        if (seq.match(f(E(i)))) {
                            toset = E(i);
                            return true;
                        }
                    }
                    return false;
                }

                static bool set_two(auto& toset, auto& src, int offset = 0) {
                    auto cast_to_num = [](unsigned char c) {
                        return c - '0';
                    };
                    auto high = cast_to_num(src[offset]);
                    auto low = cast_to_num(src[offset + 1]);
                    if (high < 0 || high > 9 || low < 0 || low > 9) {
                        return false;
                    }
                    toset += high * 10 + low;
                    return true;
                };

                static DateErr set_time(Date& date, string_t& src) {
                    auto check_size = [](auto& src) {
                        return src.size() == 2;
                    };
                    auto time = commonlib2::split<string_t, const char*, strvec_t>(src, ":");
                    if (time.size() != 3) {
                        return DateError::no_time;
                    }
                    date.hour = 0;
                    date.minute = 0;
                    date.second = 0;
                    if (!check_size(time[0])) {
                        return DateError::no_hour;
                    }
                    if (!set_two(date.hour, time[0]) || date.hour > 23) {
                        return DateError::not_hour;
                    }
                    if (!check_size(time[1])) {
                        return DateError::no_minute;
                    }
                    if (!set_two(date.minute, time[1]) || date.minute > 59) {
                        return DateError::not_minute;
                    }
                    if (!check_size(time[2])) {
                        return DateError::no_second;
                    }
                    if (!set_two(date.second, time[2]) || date.second > 61) {
                        return DateError::not_second;
                    }
                    return true;
                }

               public:
                static void replace_to_parse(string_t& raw) {
                    for (auto& i : raw) {
                        if (i == '-') {
                            i = ' ';
                        }
                    }
                }

                static DateErr parse(const string_t& raw, Date& date) {
                    auto data = helper::split<string_t, Vec>(raw, " ");
                    auto check_size = [](auto& src, size_t expect) {
                        return src.size() == expect;
                    };
                    if (data.size() != 6) {
                        return DateError::not_date;
                    }
                    if (!check_size(data[0], 4) || data[0][data[0].size() - 1] != ',') {
                        return DateError::no_dayname;
                    }
                    if (!get_expected<DayName, 7>(date.dayname, data[0], get_dayname)) {
                        return DateError::not_dayname;
                    }
                    if (!check_size(data[1], 2)) {
                        return DateError::no_day;
                    }
                    date.day = 0;
                    if (!set_two(date.day, data[1]) || date.day < 1 || date.day > 31) {
                        return DateError::not_day;
                    }
                    if (!get_expected<Month, 12>(date.month, data[2], get_month)) {
                        return DateError::not_month;
                    }
                    if (!check_size(data[3], 4)) {
                        return DateError::no_day;
                    }
                    date.year = 0;
                    if (!set_two(date.year, data[3])) {
                        return DateError::not_year;
                    }
                    date.year *= 100;
                    if (!set_two(date.year, data[3], 2) || date.year > 9999) {
                        return DateError::not_year;
                    }
                    if (auto e = set_time(date, data[4]); !e) {
                        return e;
                    }
                    if (data[5] != "GMT") {
                        return DateError::not_GMT;
                    }
                    return true;
                }
#ifndef _WIN32
#define gettime_s(tm, time) (*tm = gmtime(time))
#endif
            };

            struct UTCLocalDiff {
               private:
                static time_t diff_impl() {
                    auto calc_diff = [] {
                        time_t now_local = time(nullptr), now_utc = 0;
                        ::tm tminfo = {0};
                        gmtime_s(&tminfo, &now_local);
                        now_utc = mktime(&tminfo);
                        return now_local - now_utc;
                    };
                    return calc_diff();
                }

               public:
                static time_t diff() {
                    static time_t diff_v = diff_impl();
                    return diff_v;
                }
            };

            template <class String>
            struct DateWriter {
                using string_t = String;

               private:
                static void set_two(std::uint32_t src, string_t& towrite) {
                    auto high = src / 10;
                    auto low = src - high * 10;
                    towrite += high + '0';
                    towrite += low + '0';
                }

               public:
                static DateErr write(string_t& towrite, const Date& date) {
                    if (date.dayname == DayName::unset || date.dayname == DayName::invalid) {
                        return DateError::not_dayname;
                    }
                    towrite += get_dayname(date.dayname);
                    towrite += ", ";
                    if (date.day < 1 || date.day > 31) {
                        return DateError::not_day;
                    }
                    set_two(date.day, towrite);
                    towrite += ' ';
                    if (date.month == Month::unset || date.month == Month::invalid) {
                        return DateError::not_month;
                    }
                    towrite += get_month(date.month);
                    towrite += ' ';
                    if (date.year > 9999) {
                        return DateError::not_year;
                    }
                    set_two(date.year / 100, towrite);
                    set_two(date.year - (date.year / 100) * 100, towrite);
                    towrite += ' ';
                    if (date.hour > 23) {
                        return DateError::not_hour;
                    }
                    set_two(date.hour, towrite);
                    towrite += ':';
                    if (date.minute > 59) {
                        return DateError::not_minute;
                    }
                    set_two(date.minute, towrite);
                    towrite += ':';
                    if (date.second > 61) {
                        return DateError::not_minute;
                    }
                    set_two(date.second, towrite);
                    towrite += " GMT";
                    return true;
                }
            };

            struct TimeConvert {
                static DateErr from_time_t(time_t time, Date& date) {
                    if (time < 0) {
                        return DateError::not_date;
                    }
                    ::tm tminfo = {0};
                    gmtime_s(&tminfo, &time);
                    date.dayname = DayName(1 + tminfo.tm_wday);
                    date.day = (std::uint8_t)tminfo.tm_mday;
                    date.month = Month(1 + tminfo.tm_mon);
                    date.year = (std::uint16_t)tminfo.tm_year + 1900;
                    date.hour = (std::uint8_t)tminfo.tm_hour;
                    date.minute = (std::uint8_t)tminfo.tm_min;
                    date.second = (std::uint8_t)tminfo.tm_sec;
                    return true;
                }

                static DateErr to_time_t(time_t& time, const Date& date) {
                    ::tm tminfo = {0};
                    if (date.year < 1900) {
                        return DateError::not_year;
                    }
                    tminfo.tm_year = date.year - 1900;
                    if (date.month == Month::unset) {
                        return DateError::not_month;
                    }
                    tminfo.tm_mon = int(date.month) - 1;
                    if (date.day < 1 || date.day > 31) {
                        return DateError::not_day;
                    }
                    tminfo.tm_mday = date.day;
                    if (date.hour > 23) {
                        return DateError::not_hour;
                    }
                    tminfo.tm_hour = date.hour;
                    if (date.minute > 59) {
                        return DateError::not_minute;
                    }
                    tminfo.tm_min = date.minute;
                    if (date.second > 61) {
                        return DateError::not_second;
                    }
                    tminfo.tm_sec = date.second;
                    time = mktime(&tminfo);
                    if (time == -1) {
                        return DateError::not_date;
                    }
                    time += UTCLocalDiff::diff();
                    return true;
                }
            };

            bool operator==(const Date& a, const Date& b) {
                union {
                    Date d;
                    std::uint64_t l;
                } a_c{a}, b_c{b};
                return a_c.l == b_c.l;
            }

            time_t operator-(const Date& a, const Date& b) {
                time_t a_t = 0, b_t = 0;
                TimeConvert::to_time_t(a_t, a);
                TimeConvert::to_time_t(b_t, b);
                return a_t - b_t;
            }
        }  // namespace date

    }  // namespace net
}  // namespace utils
