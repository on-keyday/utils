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
#include "flags.h"

namespace utils {
    namespace binary {

        // msb to lsb
        template <class T>
        constexpr T bit_range(byte start, byte size) {
            T value = 0;
            auto bit = [&](auto i) { return T(1) << (sizeof(T) * bit_per_byte - 1 - i); };
            for (size_t i = 0; i < size; i++) {
                value |= bit(start + i);
            }
            return value;
        }

        template <class T, class F, byte exp_width, int bias_>
        struct IEEEFloat {
            static_assert(std::is_void_v<F> || std::is_floating_point_v<F>);
            using float_t = F;

           private:
            static constexpr auto t_bit = sizeof(T) * bit_per_byte;
            flags_t<T, 1, exp_width, byte(t_bit - 1 - exp_width)> value;

           public:
            using frac_t = T;
            using exp_t = decltype(value.template get<1>());

            static constexpr exp_t bias = bias_;
            static constexpr byte exp_bits_width = exp_width;
            static constexpr byte exp_shift = t_bit - 1 - exp_bits_width;

            static constexpr T sign_bit = bit_range<T>(0, 1);
            static constexpr T exp_bits = bit_range<T>(1, exp_bits_width);
            static constexpr T frac_bits = ~(sign_bit | exp_bits);

            static constexpr T frac_msb = bit_range<T>(t_bit - exp_shift, 1);

            constexpr IEEEFloat() = default;
            constexpr IEEEFloat(T input) noexcept {
                value.as_value() = input;
            }

            template <class V>
                requires std::is_floating_point_v<V> && (sizeof(T) == sizeof(V))
            constexpr IEEEFloat(V v)
                : IEEEFloat(std::bit_cast<T>(v)) {}

            constexpr operator T() const noexcept {
                return value.as_value();
            }

            // raw values getter/setter

            bits_flag_alias_method(value, 0, sign);
            bits_flag_alias_method(value, 1, exponent);
            bits_flag_alias_method(value, 2, fraction);

            constexpr std::make_signed_t<exp_t> biased_exponent() const noexcept {
                return exponent() - bias;
            }

            constexpr bool set_biased_exponent(std::make_signed_t<exp_t> exp) noexcept {
                return set_exponent(exp + bias);
            }

            // status

            constexpr bool has_implicit_1() const noexcept {
                return exponent() != 0;
            }

            constexpr bool is_denormalized() const noexcept {
                return exponent() == 0 && fraction() != 0;
            }

            constexpr bool is_normalized() const noexcept {
                return exponent() != 0;
            }

            // ±Inf
            constexpr bool is_infinity() const noexcept {
                return exponent() == exponent_max && fraction() == 0;
            }

            // NaN (both quiet and signaling)
            constexpr bool is_nan() const noexcept {
                return exponent() == exponent_max && fraction() != 0;
            }

            constexpr bool is_signaling_nan() const noexcept {
                return is_nan() && bool(fraction() & frac_msb);
            }

            // processed values

            constexpr T fraction_with_implicit_1() const noexcept {
                if (has_implicit_1()) {
                    return (T(1) << exp_shift) | fraction();
                }
                return fraction();
            }

            // cast

            constexpr F to_float() const noexcept
                requires(!std::is_void_v<F>)
            {
                return std::bit_cast<F>(value.as_value());
            }

            constexpr const T& to_int() const noexcept {
                return value.as_value();
            }

            constexpr T& to_int() noexcept {
                return value.as_value();
            }
        };

        using HalfFloat = IEEEFloat<std::uint16_t, void, 5, 15>;
        using SingleFloat = IEEEFloat<std::uint32_t, float, 8, 127>;
        using DoubleFloat = IEEEFloat<std::uint64_t, double, 11, 1023>;

        namespace test {
            static_assert(HalfFloat::exp_shift == 10 && SingleFloat::exp_shift == 23 && DoubleFloat::exp_shift == 52);
            constexpr bool check_ieeefloat() {
                SingleFloat single;
                single = (0xc0000000);
                if (single != 0xC0000000 ||
                    !single.sign() ||
                    single.exponent() != 0x80 ||
                    single.fraction() != 0) {
                    return false;
                }
                single = (0x7f7fffff);
                if (single != 0x7f7fffff ||
                    single.sign() ||
                    single.exponent() != 0xFE ||
                    single.fraction() != 0x7fffff) {
                    return false;
                }
                single = 1.0e10f;
                if (single != 0x501502f9 ||
                    single.sign() ||
                    single.exponent() != 0xA0 ||
                    single.fraction() != 0x1502f9) {
                    return false;
                }
                if (single.to_float() != 1.0e10f) {
                    return false;
                }
                return true;
            }

            static_assert(check_ieeefloat());
        }  // namespace test

    }  // namespace binary
}  // namespace utils
