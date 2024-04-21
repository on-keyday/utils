/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <type_traits>

namespace futils::math::num_seq {

    constexpr bool is_arithmetic_progression(auto init, auto next, auto... args) {
        using common_t = std::common_type_t<decltype(init), decltype(next), decltype(args)...>;
        constexpr auto diff = [](auto&& self, auto a, auto b, auto... other) -> common_t {
            if constexpr (sizeof...(other) == 0) {
                return b - a;
            }
            else {
                return (b - a) - self(self, b, other...);
            }
        };
        return diff(diff, init, next, args...) == 0;
    }

    static_assert(is_arithmetic_progression(1, 2, 3, 4, 5));

    constexpr bool is_geometric_progression(auto init, auto next, auto... args) {
        using common_t = std::common_type_t<decltype(init), decltype(next), decltype(args)...>;
        bool div_by_zero = false;
        constexpr auto ratio = [&](auto&& self, auto a, auto b, auto... other) -> common_t {
            if (a == 0) {
                div_by_zero = true;
                return 0;
            }
            if constexpr (sizeof...(other) == 0) {
                return b / a;
            }
            else {
                auto c = self(self, b, other...);
                if (c == 0) {
                    return 0;
                }
                return (b / a) / c;
            }
        };
        return ratio(ratio, init, next, args...) == 1;
    }

    static_assert(is_geometric_progression(1, 2, 4, 8, 16));
}  // namespace futils::math::num_seq
