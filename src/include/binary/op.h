/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// op - operation using only bit operation
#pragma once
#include <type_traits>
#include <number/array.h>

namespace futils::binary {
    template <class T = unsigned int>
        requires std::is_unsigned_v<T>
    constexpr bool add(T a, T b, T& c) {
        T carry = 0;
        c = 0;
        for (size_t i = 0; i < sizeof(T) * 8; i++) {
            T a_bit = (a >> i) & 1;
            T b_bit = (b >> i) & 1;
            T sum = a_bit ^ b_bit ^ carry;
            carry = (carry & a_bit) | ((carry ^ a_bit) & b_bit);
            c |= sum << i;
        }
        return carry == 1;
    }

    namespace test {
        constexpr auto test_add() {
            std::uint64_t a = 0x0123456789ABCDEF;
            std::uint64_t b = 0xFEDCBA9876543210;
            std::uint64_t c = 0;
            add(a, b, c);
            if (c != a + b) {
                return false;
            }
            if (!add(c, std::uint64_t(1), c)) {
                return false;
            }
            return true;
        }

        static_assert(test_add());
    }  // namespace test

    template <class T = unsigned int>
        requires std::is_unsigned_v<T>
    constexpr size_t mul(T a, T b, T& c) {
        size_t count = 0;
        c = 0;
        for (size_t i = 0; i < sizeof(T) * 8; i++) {
            if ((b >> i) & 1) {
                if (add(c, a << i, c)) {
                    count++;
                }
            }
        }
        return count;
    }

    namespace test {
        constexpr auto test_mul() {
            constexpr std::uint64_t a = 0b1111;
            constexpr std::uint64_t b = 0b1111;
            std::uint64_t c = 0;
            mul(a, b, c);
            return c;
        }

        constexpr auto result = test_mul();
    }  // namespace test

    template <class Vec>
    constexpr void add(Vec& a, std::uint64_t b) {
        if (a.size() == 0) {
            a.push_back(b);
            return;
        }
        // add b to a
        // a will be changed
        std::uint64_t carry = 0;
        for (size_t i = 0; i < a.size(); i++) {
            if (add(a[i], b, a[i])) {
                carry = 1;
            }
            else {
                carry = 0;
                break;
            }
        }
        if (carry) {
            a.push_back(1);
        }
    }

    struct uint128 {
        uint64_t low;
        uint64_t high;
    };

    constexpr uint128 multiply64To128(uint64_t a, uint64_t b) {
        uint64_t a_lo = a & 0xFFFFFFFF;
        uint64_t a_hi = a >> 32;
        uint64_t b_lo = b & 0xFFFFFFFF;
        uint64_t b_hi = b >> 32;

        uint64_t p1 = a_lo * b_lo;
        uint64_t p2 = a_lo * b_hi;
        uint64_t p3 = a_hi * b_lo;
        uint64_t p4 = a_hi * b_hi;

        uint64_t p1_hi = p1 >> 32;
        uint64_t p1_lo = p1 & 0xFFFFFFFF;

        uint64_t p2_hi = p2 >> 32;
        uint64_t p2_lo = p2 & 0xFFFFFFFF;

        uint64_t p3_hi = p3 >> 32;
        uint64_t p3_lo = p3 & 0xFFFFFFFF;

        uint64_t carry = 0;
        uint64_t sum_lo = p1_lo + (p2_lo << 32) + (p3_lo << 32);
        if (sum_lo < p1_lo) {
            carry++;
        }

        uint64_t sum_hi = p1_hi + p2_hi + p3_hi + p4 + carry;

        uint128 result;
        result.low = sum_lo;
        result.high = sum_hi;

        return result;
    }

    template <class Vec>
    constexpr void mul(Vec& a, std::uint64_t b) {
        // multiply a by b
        // a will be changed
        std::uint64_t carry = 0;
        for (size_t i = 0; i < a.size(); i++) {
            uint128 result = multiply64To128(a[i], b);
            if (add(result.low, carry, result.low)) {
                result.high++;
            }
            a[i] = result.low;
            carry = result.high;
            if (carry && i == a.size() - 1) {
                a.push_back(0);
            }
        }
    }

    namespace test {
        constexpr auto multiply = multiply64To128(0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF);

        constexpr auto mul_test() {
            futils::number::Array<std::uint64_t, 60> a{};
            constexpr auto text = "123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890";
            for (auto l = text; *l; l++) {
                mul(a, std::uint64_t(10));
                add(a, std::uint64_t(*l - '0'));
            }
            return a;
        }

        constexpr auto mul_test_result = mul_test();
    }  // namespace test

}  // namespace futils::binary
