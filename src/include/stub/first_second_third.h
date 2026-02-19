/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

namespace futils::joke {
    consteval decltype(auto) operator"" st(unsigned long long n) {
        if (n % 10 != 1 || n % 100 == 11) {
            throw "Not a first";
        }
        return n;
    }

    consteval decltype(auto) operator"" nd(unsigned long long n) {
        if (n % 10 != 2 || n % 100 == 12) {
            throw "Not a second";
        }
        return n;
    }

    consteval decltype(auto) operator"" rd(unsigned long long n) {
        if (n % 10 != 3 || n % 100 == 13) {
            throw "Not a third";
        }
        return n;
    }

    consteval decltype(auto) operator"" th(unsigned long long n) {
        if (n % 100 / 10 == 1) {
            return n;
        }
        if (n % 10 >= 1 && n % 10 <= 3) {
            throw "Not a th";
        }
        return n;
    }
    namespace test {
        constexpr auto first = 1st;
        constexpr auto second = 2nd;
        constexpr auto third = 3rd;
        constexpr auto fourth = 4th;
        constexpr auto fifth = 5th;
        constexpr auto sixth = 6th;
        constexpr auto seventh = 7th;
        constexpr auto eighth = 8th;
        constexpr auto ninth = 9th;
        constexpr auto tenth = 10th;
        constexpr auto eleventh = 11th;
        constexpr auto twelfth = 12th;
        constexpr auto thirteenth = 13th;
        constexpr auto fourteenth = 14th;
        constexpr auto fifteenth = 15th;
        constexpr auto sixteenth = 16th;
        constexpr auto seventeenth = 17th;
        constexpr auto eighteenth = 18th;
        constexpr auto nineteenth = 19th;
        constexpr auto twentieth = 20th;
        constexpr auto twenty_first = 21st;
        constexpr auto twenty_second = 22nd;
        constexpr auto twenty_third = 23rd;
        constexpr auto twenty_fourth = 24th;
        constexpr auto twenty_fifth = 25th;
        constexpr auto twenty_sixth = 26th;
        constexpr auto twenty_seventh = 27th;
        constexpr auto twenty_eighth = 28th;
        constexpr auto twenty_ninth = 29th;
        constexpr auto thirtieth = 30th;
    }  // namespace test
}  // namespace futils::joke