/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <core/byte.h>
#include <cstdint>
#include <type_traits>
#include <bit>
#include <binary/flags.h>
#if __has_include(<stdfloat>) && __cplusplus >= 202302L
#include <stdfloat>
#endif

namespace futils {
    namespace binary {

#if defined(__STDCPP_FLOAT16_T__)
        using float16_t = std::float16_t;
#else
        using float16_t = void;
#endif
#if defined(__STDCPP_BFLOAT16_T__)
        using bfloat16_t = void;
#else
        using bfloat16_t = void;
#endif
#if defined(__STDCPP_FLOAT32_T__)
        using float32_t = std::float32_t;
#else
        using float32_t = float;
#endif

#if defined(__STDCPP_FLOAT64_T__)
        using float64_t = std::float64_t;
#else
        using float64_t = double;
#endif

#if defined(__STDCPP_FLOAT128_T__)
        using float128_t = std::float128_t;
#else
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

            constexpr explicit operator T() const noexcept {
                return value.as_value();
            }

            // raw values getter/setter

            bits_flag_alias_method(value, 0, sign);
            bits_flag_alias_method(value, 1, biased_exponent);
            bits_flag_alias_method(value, 2, fraction);

            constexpr std::make_signed_t<exp_t> exponent() const noexcept {
                return biased_exponent() - bias;
            }

            constexpr bool exponent(std::make_signed_t<exp_t> exp) noexcept {
                return biased_exponent(exp + bias);
            }

            constexpr std::make_signed_t<exp_t> exponent_10_estimate() const noexcept {
                return ((exponent() + 1) / 3) - ((exponent() - 1) / 32);
            }

            // status

            constexpr bool has_implicit_1() const noexcept {
                return biased_exponent() != 0;
            }

            constexpr bool is_denormalized() const noexcept {
                return biased_exponent() == 0 && fraction() != 0;
            }

            constexpr bool is_normalized() const noexcept {
                return biased_exponent() != 0;
            }

            // ±Inf
            constexpr bool is_infinity() const noexcept {
                return biased_exponent() == biased_exponent_max && fraction() == 0;
            }

            // NaN (both quiet and signaling)
            constexpr bool is_nan() const noexcept {
                return biased_exponent() == biased_exponent_max && fraction() != 0;
            }

            constexpr bool is_quiet_nan() const noexcept {
                return is_nan() && bool(fraction() & fraction_msb);
            }

            constexpr bool is_canonical_nan() const noexcept {
                return biased_exponent() == biased_exponent_max && fraction() == fraction_msb;
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

            constexpr void make_normalized() {
                constexpr auto bits_size = sizeof(unsigned long long) * bit_per_byte;
                constexpr auto zero_prefix = bits_size - float_bits_width;
                auto clzll = [](unsigned long long x) {
                    if (std::is_constant_evaluated()) {
                        for (int i = 0; i < bits_size; i++) {
                            if (!(x & (1ull << (bits_size - 1 - i)))) {
                                return i;
                            }
                        }
                        throw "undefined";
                    }
                    else {
                        return __builtin_clzll(x);
                    }
                };
                auto shift = clzll(fraction()) - zero_prefix - exponent_bits_width;
                if (shift < 0) {
                    uint32_t rshift = (uint32_t)(-shift);
                    auto max = exponent() + rshift;
                    if (max > bias) {
                        biased_exponent(biased_exponent_max);
                        fraction(0);
                        return;
                    }
                    fraction(fraction() >> rshift);
                    exponent(max);
                }
                else if (shift > 0) {
                    auto min = exponent() - shift;
                    if (min < -(bias) + 1) {
                        biased_exponent(0);
                        fraction(0);
                        return;
                    }
                    int32_t max_shift = exponent() - (bias) + 1;
                    if (shift > max_shift) {
                        // 正規化しきると指数がアンダーフローするため、非正規化数の限界までシフト
                        if (max_shift > 0) {
                            fraction(fraction() << max_shift);
                            exponent(exponent() - max_shift);
                        }
                    }
                    else {
                        fraction(fraction() << shift);
                        exponent(min);
                    }
                }
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

        template <class Float>
        using FloatT = decltype(make_float(Float{}));

        // also canonical
        template <class Float>
        constexpr auto quiet_nan = [] {
            auto f = make_float(Float());
            f.biased_exponent(f.biased_exponent_max);
            f.fraction(f.fraction_msb);
            return f;
        }();

        template <class Float>
        constexpr auto signaling_nan = [] {
            auto f = make_float(Float());
            f.biased_exponent(f.biased_exponent_max);
            f.fraction(f.fraction_msb);
            f.make_quiet_signaling();
            return f;
        }();

        template <class Float, typename decltype(make_float(Float{}))::frac_t payload>
        constexpr auto signaling_nan_with_payload = [] {
            auto f = make_float(Float());
            f.biased_exponent(f.biased_exponent_max);
            f.fraction(f.fraction_msb);
            if (!f.make_quiet_signaling(payload)) {
                throw "invalid payload";
            }
            return f;
        }();

        template <class Float>
        constexpr auto indeterminate_nan = [] {
            auto f = make_float(Float());
            f.biased_exponent(f.biased_exponent_max);
            f.fraction(f.fraction_msb);
            f.sign(true);
            return f;
        }();

        template <class Float>
        constexpr auto infinity = [] {
            auto f = make_float(Float());
            f.biased_exponent(f.biased_exponent_max);
            return f;
        }();

        template <class Float>
        constexpr auto zero = make_float(Float());

        template <class Float>
        constexpr auto epsilon = [] {
            auto f = make_float(Float());
            f.biased_exponent(f.bias - f.fraction_bits_width);
            return f;
        }();

        // non zero minimum denormalized value (no sign)
        template <class Float>
        constexpr auto min_denormalized = [] {
            auto f = make_float(Float());
            f.biased_exponent(0);
            f.fraction(1);
            return f;
        }();

        template <class Float>
        constexpr auto max_denormalized = [] {
            auto f = make_float(Float());
            f.biased_exponent(0);
            f.fraction(f.fraction_max);
            return f;
        }();

        // non zero minimum normalized value (no sign)
        template <class Float>
        constexpr auto min_normalized = [] {
            auto f = make_float(Float());
            f.biased_exponent(1);
            return f;
        }();

        template <class Float>
        constexpr auto max_normalized = [] {
            auto f = make_float(Float());
            f.biased_exponent(f.biased_exponent_max - 1);
            f.fraction(f.fraction_max);
            return f;
        }();

        template <class Float>
        constexpr auto normalize(Float x) {
            auto f = make_float(x);
            f.make_normalized();
            return f;
        }

        namespace test {
            // static_assert(HalfFloat::exponent_shift == 10 && SingleFloat::exponent_shift == 23 && DoubleFloat::exponent_shift == 52);
            static_assert(HalfFloat::fraction_bits_width == 10 && SingleFloat::fraction_bits_width == 23 && DoubleFloat::fraction_bits_width == 52);
            constexpr bool check_ieeefloat() {
                SingleFloat single;
                single = (0xc0000000);
                if (single.to_int() != 0xC0000000 ||
                    !single.sign() ||
                    single.biased_exponent() != 0x80 ||
                    single.fraction() != 0) {
                    return false;
                }
                single = (0x7f7fffff);
                if (single.to_int() != 0x7f7fffff ||
                    single.sign() ||
                    single.biased_exponent() != 0xFE ||
                    single.fraction() != 0x7fffff) {
                    return false;
                }
                single = 1.0e10f;
                if (single.to_int() != 0x501502f9 ||
                    single.sign() ||
                    single.biased_exponent() != 0xA0 ||
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
