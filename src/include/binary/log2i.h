/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstdint>

namespace futils::binary {
    constexpr std::uint64_t n_bit_max(size_t n) {
        if (n == 0 || n > 64) {
            return 0;
        }
        constexpr auto n64_max = ~std::uint64_t(0);
        return n64_max >> (64 - n);
    }

    namespace internal {

        template <size_t s, size_t l>
        constexpr size_t log2i_impl(std::uint64_t n) {
            if constexpr (s + 1 == l) {
                constexpr auto s_max = n_bit_max(s);
                return n <= s_max ? s : l;
            }
            else {
                constexpr auto m = (s + l) >> 1;
                constexpr auto mid = n_bit_max(m);
                return n < mid ? log2i_impl<s, m>(n) : log2i_impl<m, l>(n);
            }
        }

        constexpr auto test1 = log2i_impl<0, 64>(31);
        constexpr auto test2 = log2i_impl<0, 64>(8);
        constexpr auto test3 = log2i_impl<0, 64>(0);
        constexpr auto test4 = log2i_impl<0, 64>(1);
        constexpr auto test5 = log2i_impl<0, 64>(2);
        constexpr auto test6 = log2i_impl<0, 64>(3);
        constexpr auto test7 = log2i_impl<0, 64>(0xffffffffffffffff);

        static_assert(test1 == 5);
        static_assert(test2 == 4);
        static_assert(test3 == 0);
        static_assert(test4 == 1);
        static_assert(test5 == 2);
        static_assert(test6 == 2);
        static_assert(test7 == 64);

        constexpr bool check_border() {
            for (auto i = 0; i <= 64; i++) {
                auto n = n_bit_max(i);
                if (log2i_impl<0, 64>(n) != i || log2i_impl<0, 64>(n + 1) == i) {
                    return false;
                }
            }
            return true;
        }

        static_assert(check_border());

    }  // namespace internal

    constexpr size_t log2i(std::uint64_t n) {
        return internal::log2i_impl<0, 64>(n);
    }
}  // namespace futils::binary