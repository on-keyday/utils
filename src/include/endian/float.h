/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../core/byte.h"
#include <cstdint>
#include <type_traits>
#include <bit>

namespace utils {
    namespace endian {

        template <class T>
        constexpr size_t bitsof() noexcept {
            return sizeof(T) << 3;  // sizoef(T)*8
        }

        // msb to lsb
        template <class T>
        constexpr T bit_range(byte start, byte size) {
            T value = 0;
            auto bit = [&](auto i) { return T(1) << (bitsof<T>() - 1 - i); };
            for (size_t i = 0; i < size; i++) {
                value |= bit(start + i);
            }
            return value;
        }

        template <class T, class Exp, byte exp_width, Exp bias_>
        struct IEEEFloat {
            using frac_t = T;
            using exp_t = Exp;

            static constexpr Exp bias = bias_;
            static constexpr byte exp_bits_width = exp_width;
            static constexpr byte exp_shift = bitsof<T>() - 1 - exp_bits_width;

            static constexpr T sign_bit = bit_range<T>(0, 1);
            static constexpr T exp_bits = bit_range<T>(1, exp_bits_width);
            static constexpr T frac_bits = ~(sign_bit | exp_bits);

            static constexpr Exp exp_max = Exp(exp_bits >> exp_shift);

            static constexpr T frac_msb = bit_range<T>(bitsof<T>() - exp_shift, 1);

           private:
            T frac_ = 0;
            bool sign_ = false;
            Exp exp_ = 0;

           public:
            constexpr IEEEFloat() = default;
            constexpr IEEEFloat(T input) noexcept {
                parse(input);
            }

            constexpr operator T() const noexcept {
                return compose();
            }

            // parse/compose

            constexpr void parse(T value) noexcept {
                sign_ = bool(value & sign_bit);
                exp_ = Exp((value & exp_bits) >> exp_shift);
                frac_ = (value & frac_bits);
            }

            constexpr T compose() const noexcept {
                T value = 0;
                if (sign_) {
                    value |= sign_bit;
                }
                value |= (T(exp_) << exp_shift) & exp_bits;
                value |= frac_ & frac_bits;
                return value;
            }

            // raw values

            constexpr bool sign() const noexcept {
                return sign_;
            }

            constexpr Exp exponent() const noexcept {
                return exp_;
            }

            constexpr T fraction() const noexcept {
                return frac_;
            }

            // setter

            constexpr void set_sign(bool s) noexcept {
                sign_ = s;
            }

            constexpr bool set_exponent(Exp exp) noexcept {
                if (exp & (~bit_range<Exp>(0, bitsof<Exp>()))) {
                    return false;
                }
                exp_ = exp;
            }

            constexpr bool set_biased_exponent(std::make_signed_t<Exp> exp) noexcept {
                return set_exponent(exp + bias_);
            }

            constexpr bool set_fraction(T frac) noexcept {
                if (frac & (~frac_bits)) {
                    return false;
                }
                frac_ = frac;
                return true;
            }

            // status

            constexpr bool has_implicit_1() const noexcept {
                return exp_ != 0;
            }

            constexpr bool is_denormalized() const noexcept {
                return exp_ == 0 && frac_ != 0;
            }

            constexpr bool is_normalized() const noexcept {
                return exp_ != 0;
            }

            // ±Inf
            constexpr bool is_infinity() const noexcept {
                return exp_ == exp_max && frac_ == 0;
            }

            // NaN (both quiet and signaling)
            constexpr bool is_nan() const noexcept {
                return exp_ == exp_max && frac_ != 0;
            }

            constexpr bool is_signaling_nan() const noexcept {
                return is_nan() && bool(frac_ & frac_msb);
            }

            // processed values

            constexpr std::make_signed_t<Exp> biased_exponent() const noexcept {
                return exp_ - bias_;
            }

            constexpr T fraction_with_implicit_1() const noexcept {
                if (has_implicit_1()) {
                    return (T(1) << exp_shift) | frac_;
                }
                return frac_;
            }
        };

        using HalfFloat = IEEEFloat<std::uint16_t, byte, 5, 15>;
        using SingleFloat = IEEEFloat<std::uint32_t, byte, 8, 127>;
        using DoubleFloat = IEEEFloat<std::uint64_t, std::uint16_t, 11, 1023>;

        namespace test {
            static_assert(HalfFloat::exp_shift == 10 && SingleFloat::exp_shift == 23 && DoubleFloat::exp_shift == 52);
            constexpr bool check_ieeefloat() {
                SingleFloat single;
                single.parse(0xc0000000);
                if (single.compose() != 0xC0000000 ||
                    !single.sign() ||
                    single.exponent() != 0x80 ||
                    single.fraction() != 0) {
                    return false;
                }
                single.parse(0x7f7fffff);
                if (single.compose() != 0x7f7fffff ||
                    single.sign() ||
                    single.exponent() != 0xFE ||
                    single.fraction() != 0x7fffff) {
                    return false;
                }
                return true;
            }

            static_assert(check_ieeefloat());
        }  // namespace test

    }  // namespace endian
}  // namespace utils
