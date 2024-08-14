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

    constexpr std::uint64_t bit_pair_masks[] = {
        // 0101010101010101010101010101010101010101010101010101010101010101
        std::uint64_t(0x5555555555555555),
        // 0011001100110011001100110011001100110011001100110011001100110011
        std::uint64_t(0x3333333333333333),
        // 0000111100001111000011110000111100001111000011110000111100001111
        std::uint64_t(0x0F0F0F0F0F0F0F0F),
        // 0000000011111111000000001111111100000000111111110000000011111111
        std::uint64_t(0x00FF00FF00FF00FF),
        // 0000000000000000111111111111111100000000000000001111111111111111
        std::uint64_t(0x0000FFFF0000FFFF),
        // 0000000000000000000000000000000011111111111111111111111111111111
        std::uint64_t(0x00000000FFFFFFFF),
        // 1111111111111111111111111111111111111111111111111111111111111111
        std::uint64_t(0xffffffffffffffff),
    };

    template <class T>
    constexpr auto pop_count(T v) {
        static_assert(std::is_unsigned_v<T>);
        constexpr auto count = sizeof(T);
        constexpr auto bit_mask = [](size_t i) {
            constexpr auto num_mask = ~T(0);
            return T(bit_pair_masks[i] & num_mask);
        };
        constexpr auto bit_mask_0 = bit_mask(0);
        v = v - ((v >> 1) & bit_mask_0);
        constexpr auto bit_mask_1 = bit_mask(1);
        v = (v & bit_mask_1) + ((v >> 2) & bit_mask_1);
        constexpr auto bit_mask_2 = bit_mask(2);
        v = (v + (v >> 4)) & bit_mask_2;
        if constexpr (count >= 2) {
            constexpr auto bit_mask_3 = bit_mask(3);
            v = (v + (v >> 8)) & bit_mask_3;
        }
        if constexpr (count >= 4) {
            constexpr auto bit_mask_4 = bit_mask(4);
            v = (v + (v >> 16)) & bit_mask_4;
        }
        if constexpr (count >= 8) {
            constexpr auto bit_mask_5 = bit_mask(5);
            v = (v + (v >> 32)) & bit_mask_5;
        }
        v = v & 0x3F;
        return v;
    }

    namespace test {

        constexpr auto pop_count8_test() {
            return pop_count(std::uint8_t(0b10101010));
        }

        static_assert(pop_count8_test() == 4);

        constexpr auto pop_count16_test() {
            return pop_count(std::uint16_t(0b1010101010101010));
        }

        static_assert(pop_count16_test() == 8);

        constexpr auto pop_count32_test() {
            return pop_count(0b10101010101010101010101010101010);
        }

        static_assert(pop_count32_test() == 16);

        constexpr auto pop_count64_test() {
            return pop_count(0b1010101010101010101010101010101010101010101010101010101010101010);
        }

        static_assert(pop_count64_test() == 32);
    }  // namespace test

}  // namespace futils::binary
