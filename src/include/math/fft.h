/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <helper/defer.h>
#include <math/derive.h>

namespace futils::math::fft {

    template <class T>
    struct complex {
        T real = 0;
        T imag = 0;
    };

    template <class T, class U>
    constexpr auto operator+(complex<T> a, complex<U> b) noexcept {
        return complex<std::common_type_t<T, U>>{a.real + b.real, a.imag + b.imag};
    }
    template <class T, class U>
    constexpr auto operator-(complex<T> a, complex<U> b) noexcept {
        return complex<std::common_type_t<T, U>>{a.real - b.real, a.imag - b.imag};
    }
    template <class T, class U>
    constexpr auto operator*(complex<T> a, complex<U> b) noexcept {
        return complex<std::common_type_t<T, U>>{a.real * b.real - a.imag * b.imag,
                                                 a.real * b.imag + a.imag * b.real};
    }
    template <class T, class U>
    constexpr auto operator/(complex<T> a, complex<U> b) noexcept {
        auto c = b.real * b.real + b.imag * b.imag;
        return complex<std::common_type_t<T, U>>{(a.real * b.real + a.imag * b.imag) / c,
                                                 (a.imag * b.real - a.real * b.imag) / c};
    }

    template <class T>
    struct complex_vec {
        complex<T>* data = nullptr;
        size_t size = 0;
    };

    template <class T>
    complex_vec(complex<T>*, size_t) -> complex_vec<T>;

    namespace internal {
        constexpr double abs(double x) noexcept {
            return x < 0 ? -x : x;
        }

        constexpr double ceil(binary::DoubleFloat f) noexcept {
            if (f.is_zero() || f.is_nan() || f.is_infinity()) {
                return f.to_float();
            }
            if (f.is_denormalized()) {
                auto z = binary::zero<double>;
                z.sign(f.sign());
                return z;
            }
            auto exponent = f.biased_exponent();
            if (exponent < 0) {
                return f.sign() ? 0.0 : 1.0;
            }
            auto mantissa = f.fraction();
            auto mask = f.fraction_mask >> exponent;
            if ((f.fraction() & mask) == 0) {
                return f.to_float();
            }
            if (!f.sign()) {
                mantissa += 1 << (f.fraction_bits_width - exponent);
                if (mantissa > (f.fraction_max + 1)) {
                    mantissa = 0;
                    exponent += 1;
                }
            }
            mantissa &= ~mask;
            f.biased_exponent(exponent);
            f.fraction(mantissa);
            return f.to_float();
        }

        constexpr double floor(binary::DoubleFloat f) noexcept {
            if (f.is_zero() || f.is_nan() || f.is_infinity()) {
                return f.to_float();
            }
            if (f.is_denormalized()) {
                auto z = binary::zero<double>;
                z.sign(f.sign());
                return z;
            }
            bool sign = f.sign();
            auto exponent = f.biased_exponent();
            auto mantissa = f.fraction();
            int mul_factor = 1;
            auto i = exponent < 0 ? -exponent : exponent;
            for (; i > 0; i--) {
                mul_factor *= 2;
            }
            auto denominator = uint64_t(1) << f.fraction_bits_width;
            auto numerator = f.fraction_with_implicit_1();
            if (exponent < 0) {
                denominator *= mul_factor;
            }
            else {
                numerator *= mul_factor;
            }
            double result = 0;
            while (numerator >= denominator) {
                result += 1;
                numerator -= denominator;
            }
            if (sign) {
                result = -result;
                if (numerator != 0) {
                    result -= 1;
                }
            }
            return result;
        }

        constexpr double trunc(double x) {
            if (x < 0) {
                return ceil(x);
            }
            else {
                return floor(x);
            }
        }

        constexpr double fmod(double x, double y) {
            auto chk = binary::DoubleFloat(y);
            if (chk.is_zero() || chk.is_nan() || chk.is_infinity()) {
                return binary::quiet_nan<double>;
            }
            if (chk.is_denormalized()) {
                return x;
            }
            if (y == 0 || x == 0 || x == y) {
                return 0;
            }
            if (x < 0) {
                return -fmod(-x, y);
            }
            if (y < 0) {
                return fmod(x, -y);
            }
            if (x < y) {
                return x;
            }
            auto n = trunc(x / y);
            return x - n * y;
        }

        constexpr void sin_cos_impl(double x, double* sin, double* cos) noexcept {
            constexpr auto pi = c_pi.raw().to_float();
            x = fmod(x, pi);  //+-pi?
            auto sin_now = 1.0;
            auto cos_now = 1.0;
            auto x2 = x * x;
            auto i = 1;
            double tmp_sin, tmp_cos;
            if (!sin) sin = &tmp_sin;
            if (!cos) cos = &tmp_cos;
            *sin = x;
            *cos = 1;
            for (i = 1; i <= 30; i++) {
                sin_now *= -x2 / ((double)(i << 1) * (double)((i << 1) + 1));
                cos_now *= -x2 / ((double)(i << 1) * (double)((i << 1) - 1));
                *sin += sin_now;
                *cos += cos_now;
            }
        }

        constexpr double sin(double x) noexcept {
            if (std::is_constant_evaluated()) {
                double sin;
                sin_cos_impl(x, &sin, nullptr);
                return sin;
            }
            else {
                return std::sin(x);
            }
        }

        constexpr double cos(double x) noexcept {
            if (std::is_constant_evaluated()) {
                double cos;
                sin_cos_impl(x, nullptr, &cos);
                return cos;
            }
            else {
                return std::cos(x);
            }
        }

        template <class T, class R>
        constexpr void dft_impl(complex_vec<R> result, complex_vec<T> target, bool inv) noexcept {
            if (target.size <= 1 || target.data == nullptr) {
                return;
            }
            size_t n = target.size;
            size_t n_div2 = n >> 1;
            auto tmp1 = complex_vec<T>{new complex<T>[n_div2], n_div2};
            auto tmp2 = complex_vec<T>{new complex<T>[n_div2], n_div2};
            const auto d = futils::helper::defer([&] {
                delete[] tmp1.data;
                delete[] tmp2.data;
            });
            for (size_t i = 0; i < n_div2; i++) {
                tmp1.data[i] = target.data[i << 1];
                tmp2.data[i] = target.data[(i << 1) + 1];
            }
            dft_impl(tmp1, tmp1, inv);
            dft_impl(tmp2, tmp2, inv);
            complex<T> now = {1, 0};
            constexpr auto pi2 = 2 * c_pi.raw().to_float();
            complex<double> base = {cos(pi2 / n), sin(pi2 / n)};
            if (inv) {
                base.imag = -base.imag;
            }
            for (size_t i = 0; i < n; i++) {
                auto res = tmp1.data[i % n_div2] + now * tmp2.data[i % n_div2];
                result.data[i].real = res.real;
                result.data[i].imag = res.imag;
                auto new_now = now * base;
                now.real = new_now.real;
                now.imag = new_now.imag;
            }
        }
    }  // namespace internal

    // target.size must be power of 2
    template <class T, class R>
    constexpr bool dft(complex_vec<R> result, complex_vec<T> target) noexcept {
        if (target.size == 0 || target.data == nullptr ||
            (target.size & (target.size - 1)) != 0) {
            return false;
        }
        if (!result.data || result.size != target.size) {
            return false;
        }
        internal::dft_impl(result, target, false);
        return true;
    }

    // target.size must be power of 2
    template <class T, class R>
    constexpr bool idft(complex_vec<R> result, complex_vec<T> target) noexcept {
        if (target.size == 0 || target.data == nullptr ||
            (target.size & (target.size - 1)) != 0) {
            return false;
        }
        if (!result.data || result.size != target.size) {
            return false;
        }
        internal::dft_impl(result, target, true);
        for (size_t i = 0; i < target.size; i++) {
            result.data[i].real /= target.size;
            result.data[i].imag /= target.size;
        }
        return true;
    }

    namespace test {
        constexpr bool test_dft() {
            constexpr auto pi = c_pi.raw().to_float();
            constexpr auto eps = 1e-5;
            constexpr auto n = 1 << 10;
            complex<double> data1[n];
            complex<double> data2[n];
            complex<double> data3[n];
            for (size_t i = 0; i < n; i++) {
                data1[i].real = internal::cos(2 * pi * i / n);
                data1[i].imag = internal::sin(2 * pi * i / n);
            }
            auto vec = complex_vec{data1, n};
            auto res = complex_vec{data2, n};
            auto inv = complex_vec{data3, n};
            if (!dft(res, vec)) {
                throw "";
            }
            if (!idft(inv, res)) {
                throw "";
            }
            for (size_t i = 0; i < n; i++) {
                if (internal::abs(data1[i].real - data3[i].real) > eps ||
                    internal::abs(data1[i].imag - data3[i].imag) > eps) {
                    [](auto expect, auto actual) {
                        throw "";
                    }(data1[i], data3[i]);
                }
            }
            return true;
        }

        // static_assert(test_dft());
    }  // namespace test

}  // namespace futils::math::fft
