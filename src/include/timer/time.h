/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstdint>
#include <compare>
#include <cassert>

namespace futils::timer {

    constexpr auto nano_per_sec = 1000000000;
    constexpr auto micro_per_sec = 1000000;
    constexpr auto milli_per_sec = 1000;
    constexpr auto nano_per_milli = 1000000;
    constexpr auto nano_per_micro = 1000;
    constexpr auto sec_per_year = 31536000;
    constexpr auto sec_per_day = 86400;
    constexpr auto sec_per_hour = 3600;
    constexpr auto sec_per_minute = 60;

    struct Time {
       private:
        std::uint64_t second_ = 0;
        std::uint32_t nanosecond_ = 0;

       public:
        constexpr Time() = default;
        constexpr Time(std::uint64_t second, std::uint32_t nanosecond)
            : second_(second), nanosecond_(nanosecond) {
            if (nanosecond_ >= nano_per_sec) {
                second_ += nanosecond_ / nano_per_sec;
                nanosecond_ %= nano_per_sec;
            }
        }

        constexpr std::uint64_t second() const {
            return second_;
        }

        constexpr std::uint64_t nanosecond() const {
            return nanosecond_;
        }

        constexpr std::uint64_t to_millisecond() const {
            return second_ * milli_per_sec + nanosecond_ / nano_per_milli;
        }

        constexpr std::uint64_t to_microsecond() const {
            return second_ * micro_per_sec + nanosecond_ / nano_per_micro;
        }

        constexpr std::uint64_t to_nanosecond() const {
            return second_ * nano_per_sec + nanosecond_;
        }

        constexpr friend bool operator==(const Time& left, const Time& right) {
            return left.second_ == right.second_ &&
                   left.nanosecond_ == right.nanosecond_;
        }

        constexpr friend auto operator<=>(const Time& left, const Time& right) {
            if (left.second_ != right.second_) {
                return left.second_ <=> right.second_;
            }
            return left.nanosecond_ <=> right.nanosecond_;
        }

        constexpr friend Time operator+(const Time& left, const Time& right) {
            auto nsec = left.nanosecond_ + right.nanosecond_;
            auto sec = left.second_ + right.second_ + nsec / nano_per_sec;
            nsec %= nano_per_sec;
            return {sec, nsec};
        }

        constexpr bool out_of_unix_time() const {
            return second_ > 0x7FFFFFFFFFFFFFFF;
        }
    };

    constexpr auto is_leap_year(std::uint32_t year) {
        return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    }

    constexpr auto count_leap_years(std::uint32_t begin, std::uint32_t end) {
        std::uint32_t count = 0;
        for (auto i = begin; i < end; i++) {
            if (is_leap_year(i)) {
                count++;
            }
        }
        return count;
    }

    constexpr std::uint8_t days_per_month[12] = {31, 28, 31, 30, 31, 30,
                                                 31, 31, 30, 31, 30, 31};

    constexpr std::uint16_t sum_of_days_per_month[12] = {
        0, 31, 59, 90, 120, 151,
        181, 212, 243, 273, 304, 334};

    struct TimeOrigin {
        std::int64_t year = 0;
        std::int64_t month = 0;
        std::int64_t day = 0;
        std::int64_t hour = 0;
        std::int64_t minute = 0;
        std::int64_t second = 0;
        std::int32_t nanosecond = 0;

        constexpr bool valid() const {
            if (month < 1 || month > 12) {
                return false;
            }
            if (day < 1 || day > days_per_month[month - 1]) {
                if (!is_leap_year(year) || month != 2 || day != 29) {
                    return false;
                }
            }
            if (hour > 23 || minute > 59 || second > 59) {
                return false;
            }
            if (nanosecond >= nano_per_sec) {
                return false;
            }
            return true;
        }

        constexpr TimeOrigin normalize() const {
            auto o = *this;
            if (o.nanosecond >= nano_per_sec) {
                o.second += o.nanosecond / nano_per_sec;
                o.nanosecond %= nano_per_sec;
            }
            if (o.second >= sec_per_minute) {
                o.minute += o.second / sec_per_minute;
                o.second %= sec_per_minute;
            }
            if (o.minute >= 60) {
                o.hour += o.minute / 60;
                o.minute %= 60;
            }
            if (o.hour >= 24) {
                o.day += o.hour / 24;
                o.hour %= 24;
            }
            while (o.day > days_per_month[o.month - 1]) {
                if (o.month == 2 && o.day == 29 && is_leap_year(o.year)) {
                    break;
                }
                o.day -= days_per_month[o.month - 1];
                o.month++;
                if (o.month > 12) {
                    o.year++;
                    o.month = 1;
                }
            }
            return o;
        }

        constexpr TimeOrigin from_time(const Time& t) const {
            auto sec = t.second();
            auto nsec = t.nanosecond();
            assert(nsec < nano_per_sec);
            TimeOrigin o = *this;
            o.nanosecond += nsec;
            if (o.nanosecond >= nano_per_sec) {
                o.second += o.nanosecond / nano_per_sec;
                o.nanosecond %= nano_per_sec;
            }
            o.second += sec;
            if (o.second >= sec_per_minute) {
                o.minute += o.second / sec_per_minute;
                o.second %= sec_per_minute;
            }
            if (o.minute >= 60) {
                o.hour += o.minute / 60;
                o.minute %= 60;
            }
            if (o.hour >= 24) {
                o.day += o.hour / 24;
                o.hour %= 24;
            }
            while (o.day > days_per_month[o.month - 1]) {
                if (o.month == 2 && o.day == 29 && is_leap_year(o.year)) {
                    break;
                }
                o.day -= days_per_month[o.month - 1];
                if (o.month == 2 && is_leap_year(o.year)) {
                    o.day--;
                }
                o.month++;
                if (o.month > 12) {
                    o.year++;
                    o.month = 1;
                }
            }
            return o;
        }

        constexpr friend auto operator==(const TimeOrigin& a, const TimeOrigin& b) {
            return a.year == b.year &&
                   a.month == b.month &&
                   a.day == b.day &&
                   a.hour == b.hour &&
                   a.minute == b.minute &&
                   a.second == b.second &&
                   a.nanosecond == b.nanosecond;
        }

        constexpr friend auto operator<=>(const TimeOrigin& a, const TimeOrigin& b) {
            if (a.year != b.year) {
                return a.year <=> b.year;
            }
            if (a.month != b.month) {
                return a.month <=> b.month;
            }
            if (a.day != b.day) {
                return a.day <=> b.day;
            }
            if (a.hour != b.hour) {
                return a.hour <=> b.hour;
            }
            if (a.minute != b.minute) {
                return a.minute <=> b.minute;
            }
            if (a.second != b.second) {
                return a.second <=> b.second;
            }
            return a.nanosecond <=> b.nanosecond;
        }

        constexpr friend TimeOrigin operator-(const TimeOrigin& a, const TimeOrigin& b) {
            if (a < b) {
                auto abs = b - a;
                return TimeOrigin{-abs.year, -abs.month, -abs.day, -abs.hour, -abs.minute, -abs.second, -abs.nanosecond};
            }
            TimeOrigin res;
            res.nanosecond = a.nanosecond - b.nanosecond;
            if (res.nanosecond < 0) {
                res.second--;
                res.nanosecond += nano_per_sec;
            }
            res.second += a.second - b.second;
            if (res.second < 0) {
                res.minute--;
                res.second += sec_per_minute;
            }
            res.minute += a.minute - b.minute;
            if (res.minute < 0) {
                res.hour--;
                res.minute += 60;
            }
            res.hour += a.hour - b.hour;
            if (res.hour < 0) {
                res.day--;
                res.hour += 24;
            }
            res.day += a.day - b.day;
            while (res.day < 0) {
                res.month--;
                if (res.month < 1) {
                    res.year--;
                    res.month = 12;
                }
                res.day += days_per_month[res.month - 1];
                if (res.month == 2 && is_leap_year(res.year)) {
                    res.day++;
                }
            }
            res.month += a.month - b.month;
            while (res.month < 0) {
                res.year--;
                res.month += 12;
            }
            res.year += a.year - b.year;
            return res;
        }

        constexpr Time to_zero_origin_time() const {
            std::uint64_t sec = 0;
            std::uint32_t nsec = 0;
            for (auto i = 0; i < year; i++) {
                sec += sec_per_year;
                if (is_leap_year(i)) {
                    sec += sec_per_day;
                }
            }
            sec += sum_of_days_per_month[month - 1] * sec_per_day;
            if (month > 2 && is_leap_year(year)) {
                sec += sec_per_day;
            }
            sec += (day - 1) * sec_per_day;
            sec += hour * sec_per_hour;
            sec += minute * sec_per_minute;
            sec += second;
            nsec = nanosecond;
            return {sec, nsec};
        }

        constexpr Time to_time(const TimeOrigin& target) const {
            auto origin_zero_origin = to_zero_origin_time();
            auto target_zero_origin = target.to_zero_origin_time();
            auto sec = target_zero_origin.second() - origin_zero_origin.second();
            auto nsec = target_zero_origin.nanosecond() - origin_zero_origin.nanosecond();
            if (nsec < 0) {
                sec--;
                nsec += nano_per_sec;
            }
            return {sec, static_cast<std::uint32_t>(nsec)};
        }
    };

    constexpr auto unix_epoch = TimeOrigin{1970, 1, 1, 0, 0, 0, 0};
    constexpr auto gregorian_epoch = TimeOrigin{0, 1, 1, 0, 0, 0, 0};

    namespace test {
        constexpr auto test_time_origin() {
            constexpr auto unix_epoch = TimeOrigin{1970, 1, 1, 0, 0, 0, 0};
            constexpr auto random_epoch = TimeOrigin{2000, 1, 1, 13, 42, 36, 12};
            constexpr auto zero_epoch = TimeOrigin{0, 1, 1, 0, 0, 0, 0};
            constexpr auto time_after_30sec = Time{30, 0};
            constexpr auto time_after_1min = Time{60, 0};
            constexpr auto time_after_1hour = Time{3600, 0};
            constexpr auto time_after_1day = Time{86400, 0};
            constexpr auto time_after_1month = Time{2592000, 0};
            constexpr auto time_after_1year = Time{31536000, 0};
            constexpr auto time_after_4year = Time{126230400, 0};
            constexpr auto time_after_100year = Time{3153600000, 0};
            constexpr auto time_after_400year = Time{12623040000, 0};
            constexpr auto time_after_1000year = Time{31536000000, 0};

            constexpr auto applied_1 = unix_epoch.from_time(time_after_30sec);
            constexpr auto applied_2 = unix_epoch.from_time(time_after_1min);
            constexpr auto applied_3 = unix_epoch.from_time(time_after_1hour);
            constexpr auto applied_4 = unix_epoch.from_time(time_after_1day);
            constexpr auto applied_5 = unix_epoch.from_time(time_after_1month);
            constexpr auto applied_6 = unix_epoch.from_time(time_after_1year);
            constexpr auto applied_7 = unix_epoch.from_time(time_after_4year);
            constexpr auto applied_8 = unix_epoch.from_time(time_after_100year);
            constexpr auto applied_9 = unix_epoch.from_time(time_after_400year);
            constexpr auto applied_10 = unix_epoch.from_time(time_after_1000year);

            constexpr auto applied_11 = random_epoch.from_time(time_after_30sec);
            constexpr auto applied_12 = random_epoch.from_time(time_after_1min);
            constexpr auto applied_13 = random_epoch.from_time(time_after_1hour);
            constexpr auto applied_14 = random_epoch.from_time(time_after_1day);
            constexpr auto applied_15 = random_epoch.from_time(time_after_1month);
            constexpr auto applied_16 = random_epoch.from_time(time_after_1year);
            constexpr auto applied_17 = random_epoch.from_time(time_after_4year);
            constexpr auto applied_18 = random_epoch.from_time(time_after_100year);
            constexpr auto applied_19 = random_epoch.from_time(time_after_400year);
            constexpr auto applied_20 = random_epoch.from_time(time_after_1000year);

            constexpr auto applied_21 = zero_epoch.from_time(time_after_30sec);
            constexpr auto applied_22 = zero_epoch.from_time(time_after_1min);
            constexpr auto applied_23 = zero_epoch.from_time(time_after_1hour);
            constexpr auto applied_24 = zero_epoch.from_time(time_after_1day);
            constexpr auto applied_25 = zero_epoch.from_time(time_after_1month);
            constexpr auto applied_26 = zero_epoch.from_time(time_after_1year);
            constexpr auto applied_27 = zero_epoch.from_time(time_after_4year);
            constexpr auto applied_28 = zero_epoch.from_time(time_after_100year);
            constexpr auto applied_29 = zero_epoch.from_time(time_after_400year);
            constexpr auto applied_30 = zero_epoch.from_time(time_after_1000year);

            constexpr auto applied_31 = applied_1 - applied_2;
            constexpr auto applied_32 = applied_2 - applied_3;
            constexpr auto applied_33 = applied_3 - applied_4;
            constexpr auto applied_34 = applied_4 - applied_5;
            constexpr auto applied_35 = applied_5 - applied_6;
            constexpr auto applied_36 = applied_6 - applied_7;
            constexpr auto applied_37 = applied_7 - applied_8;
            constexpr auto applied_38 = applied_8 - applied_9;
            constexpr auto applied_39 = applied_9 - applied_10;
            constexpr auto applied_40 = applied_10 - applied_11;
            return true;
        }

        static_assert(test_time_origin());
    }  // namespace test

}  // namespace futils::timer
