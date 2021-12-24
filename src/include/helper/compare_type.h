/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// compare_type - definition of compare type
#pragma once

#include "../core/sequencer.h"

namespace utils {
    namespace helper {
        template <class T, class U>
        using compare_type = typename Sequencer<buffer_t<T&>>::template compare_type<U>;

        template <class T, class U>
        constexpr auto default_compare() {
            return Sequencer<buffer_t<T&>>::template default_compare<U>();
        }

        template <class T>
        constexpr T to_upper(T&& c) {
            if (c >= 'a' && c <= 'z') {
                c = c - 'a' + 'A';
            }
            return c;
        };

        template <class T>
        constexpr T to_lower(T&& c) {
            if (c >= 'A' && c <= 'Z') {
                c = c - 'A' + 'a';
            }
            return c;
        };

        constexpr auto ignore_case() {
            return [](auto&& a, auto&& b) {
                return to_upper(a) == to_upper(b);
            };
        }
    }  // namespace helper
}  // namespace utils