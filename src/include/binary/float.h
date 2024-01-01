/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../core/byte.h"
#include <cstdint>
#include <type_traits>
#include <bit>
#include "flags.h"
#if __has_include(<stdfloat>)
#include <stdfloat>
#define FUTILS_BINARY_SUPPORT_STDFLOAT 1
#endif

namespace futils {
    namespace binary {
#ifdef FUTILS_BINARY_SUPPORT_STDFLOAT
        using bfloat16_t = std::bfloat16_t;
        using float16_t = std::float16_t;
        using float32_t = std::float32_t;
        using float64_t = std::float64_t;
        using float128_t = std::float128_t;
#else

        using bfloat16_t = void;
        using float16_t = void;
        using float32_t = float;
        using float64_t = double;
        using float128_t = void;
#endif

        template <class T, class F, byte exp_width, int bias_>
        struct Float {
            static_assert(std::is_void_v<F> || std::is_floating_point_v<F>);
            using float_t = F;

            static constexpr auto float_bits_width = sizeof(T) * bit_per_byte;
            static constexpr byte exponent_bits_width = exp_width;
            static constexpr byte fraction_bits_width = float_bits_width - 1 - exponent_bits_width;

           private:
            using Rep = flags_t<T, 1, exponent_bits_width, fraction_bits_width>;
            Rep value;

            static_assert(1 + exponent_bits_width + fraction_bits_width == float_bits_width);

            static constexpr byte exponent_shift = fraction_bits_width;

           public:
            using frac_t = T;
            using exp_t = decltype(value.template get<1>());

            static constexpr exp_t bias = bias_;

            static constexpr T fraction_msb = T(1) << (fraction_bits_width - 1);

            constexpr Float() = default;
            constexpr Float(T input) noexcept {
                value.as_value() = input;
            }

            template <class V>
                requires std::is_floating_point_v<V> && (sizeof(T) == sizeof(V))
            constexpr Float(V v)
                : Float(std::bit_cast<T>(v)) {}

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

            constexpr bool biased_exponent(std::make_signed_t<exp_t> exp) noexcept {
                return exponent(exp + bias);
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

            constexpr bool is_quiet_nan() const noexcept {
                return is_nan() && bool(fraction() & fraction_msb);
            }

            constexpr bool is_canonical_nan() const noexcept {
                return exponent() == exponent_max && fraction() == fraction_msb;
            }

            constexpr bool is_indeterminate_nan() const noexcept {
                return is_canonical_nan() && sign();
            }

            constexpr bool is_arithmetic_nan() const noexcept {
                return bool(fraction() & fraction_msb);
            }

            constexpr bool is_signaling_nan() const noexcept {
                return is_nan() && !is_quiet_nan();
            }

            constexpr bool make_quiet_signaling(frac_t bit = 1) {
                if (is_quiet_nan()) {
                    if (bit & fraction_msb) {
                        return false;
                    }
                    return fraction((fraction() & ~fraction_msb) | bit /*to be nan*/);
                }
                return false;
            }

            constexpr bool make_signaling_quiet() {
                if (is_signaling_nan()) {
                    return set_fraction((fraction() | fraction_msb));
                }
                return true;
            }

            // ±0
            constexpr bool is_zero() const noexcept {
                return fraction() == 0 && exponent() == 0;
            }

            // processed values

            constexpr T fraction_with_implicit_1() const noexcept {
                if (has_implicit_1()) {
                    return (T(1) << exponent_shift) | fraction();
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

            // values
        };

        using HalfFloat = Float<std::uint16_t, float16_t, 5, 15>;
        using BrainHalfFloat = Float<std::uint16_t, bfloat16_t, 8, 127>;
        using SingleFloat = Float<std::uint32_t, float32_t, 8, 127>;
        using DoubleFloat = Float<std::uint64_t, float64_t, 11, 1023>;
#ifdef FUTILS_BINARY_SUPPORT_INT128
        using ExtDoubleFloat = Float<uint128_t, float128_t, 15, 16383>;
#endif

        template <class F>
        struct is_Float_type_t : std::false_type {};

        template <class T, class F, byte exp, int bias>
        struct is_Float_type_t<Float<T, F, exp, bias>> : std::true_type {};

        template <class T>
        concept is_Float_type = is_Float_type_t<T>::value;

        template <class T>
        constexpr auto make_float(T t) noexcept {
            if constexpr (is_Float_type<T>) {
                return t;
            }
            else if constexpr (sizeof(T) == sizeof(HalfFloat)) {
                if constexpr (std::is_same_v<T, bfloat16_t>) {
                    return BrainHalfFloat(t);
                }
                else {
                    return HalfFloat(t);
                }
            }
            else if constexpr (sizeof(T) == sizeof(SingleFloat)) {
                return SingleFloat(t);
            }
            else if constexpr (sizeof(T) == sizeof(DoubleFloat)) {
                return DoubleFloat(t);
            }
#ifdef FUTILS_BINARY_SUPPORT_INT128
            else if constexpr (sizeof(T) == sizeof(ExtDoubleFloat)) {
                return ExtDoubleFloat(t);
            }
#endif
            else {
                static_assert(sizeof(T) != sizeof(T), "unsupported float type");
            }
        }

        // also canonical
        template <class Float>
        constexpr auto quiet_nan = [] {
            auto f = make_float(Float());
            f.exponent(f.exponent_max);
            f.fraction(f.fraction_msb);
            return f;
        }();

        template <class Float>
        constexpr auto indeterminate_nan = [] {
            auto f = make_float(Float());
            f.exponent(f.exponent_max);
            f.fraction(f.fraction_msb);
            f.sign(true);
            return f;
        }();

        template <class Float>
        constexpr auto infinity = [] {
            auto f = make_float(Float());
            f.exponent(f.exponent_max);
            return f;
        }();

        template <class Float>
        constexpr auto zero = make_float(Float());

        template <class Float>
        constexpr auto epsilon = [] {
            auto f = make_float(Float());
            f.exponent(f.bias - f.fraction_bits_width);
            return f;
        }();

        // non zero minimum denormalized value (no sign)
        template <class Float>
        constexpr auto min_denormalized = [] {
            auto f = make_float(Float());
            f.exponent(0);
            f.fraction(1);
            return f;
        }();

        // non zero minimum normalized value (no sign)
        template <class Float>
        constexpr auto min_normalized = [] {
            auto f = make_float(Float());
            f.exponent(1);
            return f;
        }();

        namespace test {
            // static_assert(HalfFloat::exponent_shift == 10 && SingleFloat::exponent_shift == 23 && DoubleFloat::exponent_shift == 52);
            static_assert(HalfFloat::fraction_bits_width == 10 && SingleFloat::fraction_bits_width == 23 && DoubleFloat::fraction_bits_width == 52);
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
}  // namespace futils
