/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// compare_type - definition of compare type
#pragma once

#include "../core/sequencer.h"

namespace utils {
    namespace strutil {
        template <class T, class U>
        using compare_type = typename Sequencer<buffer_t<T&>>::template compare_type<U>;

        constexpr auto default_compare() {
            return [](auto&& a, auto&& b) {
                return a == b;
            };
        }

        template <class T>
        constexpr std::remove_cvref_t<T> to_upper(T&& c) {
            if (c >= 'a' && c <= 'z') {
                return c - 'a' + 'A';
            }
            return c;
        };

        template <class T>
        constexpr std::remove_cvref_t<T> to_lower(T&& c) {
            if (c >= 'A' && c <= 'Z') {
                return c - 'A' + 'a';
            }
            return c;
        };

        constexpr auto ignore_case() {
            return [](auto&& a, auto&& b) {
                return to_upper(a) == to_upper(b);
            };
        }

        constexpr auto no_check(bool ret = true) {
            return [=](auto&&) -> bool {
                return ret;
            };
        }

        constexpr auto no_check2(bool ret = true) {
            return [=](auto&&, auto&&) -> bool {
                return ret;
            };
        }

    }  // namespace strutil
}  // namespace utils
