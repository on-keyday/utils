/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstddef>
#include <type_traits>
#include <binary/float.h>

namespace futils::math {

    template <class T>
    constexpr T sqrt(T t, size_t iterations = 10) {
        auto v = binary::make_float(t);
        if (v.is_nan()) {
            return t;
        }
        if (v.sign()) {
            return binary::quiet_nan<T>.to_float();
        }
        if (t == 0) {
            return 0;
        }
        T x = t / 2.0;

        for (int i = 0; i < iterations; ++i) {
            x = (x + t / x) / 2.0;
        }

        return x;
    }

    template <size_t n, class Type = double>
    struct Vector {
        Type elements[n]{};

        constexpr Type& operator[](size_t i) {
            return elements[i];
        }

        constexpr const Type& operator[](size_t i) const {
            return elements[i];
        }

        static constexpr void for_each(auto&& fn) {
            for (size_t i = 0; i < n; i++) {
                fn(i);
            }
        }

        static constexpr Vector make_for_each(auto&& fn) {
            Vector v;
            for_each([&](size_t i) { v[i] = fn(i); });
            return v;
        }

        constexpr Vector operator-() const {
            return make_for_each([&](size_t i) { return -elements[i]; });
        }

        constexpr friend Vector operator+(const Vector& a, const Vector& b) {
            return make_for_each([&](size_t i) { return a[i] + b[i]; });
        }

        constexpr friend Vector operator-(const Vector& a, const Vector& b) {
            return make_for_each([&](size_t i) { return a[i] - b[i]; });
        }

        constexpr friend Vector operator*(const Vector& a, Type b) {
            return make_for_each([&](size_t i) { return a[i] * b; });
        }

        constexpr friend Vector operator*(Type a, const Vector& b) {
            return b * a;
        }

        constexpr friend Vector operator/(const Vector& a, Type b) {
            return a * (1 / b);
        }

        // inner product
        constexpr friend Type dot(const Vector& a, const Vector& b) {
            Type v{};
            for_each([&](size_t i) {
                v += a[i] * b[i];
            });
            return v;
        }

        // cross product
        constexpr friend Vector cross(const Vector& a, const Vector& b)
            requires(n == 3)
        {
            Vector v;
            v.elements[0] = a.elements[1] * b.elements[2] - a.elements[2] * b.elements[1];
            v.elements[1] = a.elements[2] * b.elements[0] - a.elements[0] * b.elements[2];
            v.elements[2] = a.elements[0] * b.elements[1] - a.elements[1] * b.elements[0];
            return v;
        }

        constexpr Type length_squared() const {
            return dot(*this, *this);
        }

        constexpr Type length(size_t iteration = 10) const {
            return sqrt(length_squared(), iteration);
        }

        constexpr Vector normalize(size_t iteration = 10) const {
            return (*this) / length(iteration);
        }

        constexpr size_t size() const {
            return n;
        }

        constexpr auto begin() {
            return elements;
        }

        constexpr auto end() {
            return elements + n;
        }

        constexpr auto begin() const {
            return elements;
        }

        constexpr auto end() const {
            return elements + n;
        }

        constexpr bool has(auto&& fn) const {
            for (auto& elem : elements) {
                if constexpr (std::is_floating_point_v<Type>) {
                    auto f = binary::make_float(elem);
                    if (fn(f)) {
                        return true;
                    }
                }
                else {
                    if (fn(elem)) {
                        return true;
                    }
                }
            }
            return false;
        }

        constexpr bool has_nan() const
            requires(std::is_floating_point_v<Type>)
        {
            return has([](auto f) { return f.is_nan(); });
        }

        constexpr bool has_infinity() const
            requires(std::is_floating_point_v<Type>)
        {
            return has([](auto f) { return f.is_infinity(); });
        }

        constexpr bool all(auto&& fn) const {
            for (auto& elem : elements) {
                if constexpr (std::is_floating_point_v<Type>) {
                    auto f = binary::make_float(elem);
                    if (fn(f)) {
                        return false;
                    }
                }
                else {
                    if (fn(elem)) {
                        return false;
                    }
                }
            }
            return true;
        }

        constexpr friend bool operator==(const Vector& a, const Vector& b) {
            for (size_t i = 0; i < n; i++) {
                if (a[i] != b[i]) {
                    return false;
                }
            }
            return true;
        }
    };

    namespace test {
        constexpr auto vector1 = Vector<3>{1.0, 2.0, 3.0};
        constexpr auto vector2 = Vector<3>{4, 5, 6};
        constexpr auto add = vector1 + vector2;
        constexpr auto sub = vector1 - vector2;
        constexpr auto scalar_mul = vector1 * 2 + vector2 * 3;
        constexpr auto iprod = dot(add, sub);
        constexpr auto cprod = cross(sub, scalar_mul);
        constexpr auto norm = vector1.length();
        constexpr auto invalid_vector = Vector<2>{binary::quiet_nan<double>.to_float(), 1.1};
        constexpr auto has_nan = invalid_vector.has_nan();
        static_assert(has_nan);
    }  // namespace test

    template <class T = double>
    using Vec2 = Vector<2, T>;
    template <class T = double>
    using Vec3 = Vector<3, T>;
    template <class T = double>
    using Vec4 = Vector<4, T>;

}  // namespace futils::math
