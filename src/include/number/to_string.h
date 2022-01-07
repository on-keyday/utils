/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// to_string - number to string
#pragma once

#include <limits>

#include "char_range.h"

namespace utils {
    namespace number {

        template <class T>
        constexpr T radix_base_max(std::uint32_t radix) {
            auto mx = (std::numeric_limits<T>::max)();
            size_t count = 0;
            T t = 1;
            mx /= radix;
            while (mx) {
                mx /= radix;
                t *= radix;
            }
            return t;
        }

        template <class T>
        constexpr T radix_max_cache[37] = {
            0,
            0,
            radix_base_max<T>(2),
            radix_base_max<T>(3),
            radix_base_max<T>(4),
            radix_base_max<T>(5),
            radix_base_max<T>(6),
            radix_base_max<T>(7),
            radix_base_max<T>(8),
            radix_base_max<T>(9),
            radix_base_max<T>(10),
            radix_base_max<T>(11),
            radix_base_max<T>(12),
            radix_base_max<T>(13),
            radix_base_max<T>(14),
            radix_base_max<T>(15),
            radix_base_max<T>(16),
            radix_base_max<T>(17),
            radix_base_max<T>(18),
            radix_base_max<T>(19),
            radix_base_max<T>(20),
            radix_base_max<T>(21),
            radix_base_max<T>(22),
            radix_base_max<T>(23),
            radix_base_max<T>(24),
            radix_base_max<T>(25),
            radix_base_max<T>(26),
            radix_base_max<T>(27),
            radix_base_max<T>(28),
            radix_base_max<T>(29),
            radix_base_max<T>(30),
            radix_base_max<T>(31),
            radix_base_max<T>(32),
            radix_base_max<T>(33),
            radix_base_max<T>(34),
            radix_base_max<T>(35),
            radix_base_max<T>(36),
        };

        template <class Result, class T>
        constexpr NumErr to_string(Result& result, T in, int radix = 10, bool upper = false) {
            if (radix < 2 || radix > 36) {
                return NumError::invalid;
            }
            auto mx = radix_max_cache<T>[radix];
            bool first = true;
            bool sign = false;
            std::make_unsigned_t<T> calc;
            if (in < 0) {
                calc = -in;
                result.push_back('-');
            }
            else {
                calc = in;
            }
            if (calc == 0) {
                result.push_back('0');
                return true;
            }
            auto modulo = mx;
            std::make_unsigned_t<T> minus = 0;
            while (modulo) {
                auto d = (calc - minus) / modulo;
                minus += modulo * d;
                modulo /= radix;
                if (d || !first) {
                    result.push_back(to_num_char(d, upper));
                    first = false;
                }
            }
            return true;
        }

        template <class Result, class T>
        constexpr Result to_string(T in, int radix = 10, bool upper = false) {
            Result result;
            to_string(result, in, radix, upper);
            return result;
        }

        // reference implementation
        // https://blog.benoitblanchon.fr/lightweight-float-to-string/

        namespace internal {
#define VALUE(v)          \
    if (value >= 1e##v) { \
        value /= 1e##v;   \
        exp += ##v;       \
    }
            constexpr int normalize(double& value, double positive, double negative) {
                int exp = 0;
                if (value >= positive) {
                    VALUE(256)
                    VALUE(128)
                    VALUE(64)
                    VALUE(32)
                    VALUE(16)
                    VALUE(8)
                    VALUE(4)
                    VALUE(2)
                    VALUE(1)
                }
#undef VALUE
#define VALUE(v1, v2)     \
    if (value < 1e##v1) { \
        value *= 1e##v2;  \
        exp -= ##v2;      \
    }

                if (value > 0 && value <= negative) {
                    VALUE(-255, 256)
                    VALUE(-127, 128)
                    VALUE(-63, 64)
                    VALUE(-31, 32)
                    VALUE(-15, 16)
                    VALUE(-7, 8)
                    VALUE(-3, 4)
                    VALUE(-1, 2)
                    VALUE(0, 1)
                }
#undef VALUE
                return exp;
            }

            constexpr void split_float(double value, std::uint32_t& integ, std::uint32_t& decimal, std::int16_t& exp,
                                       double pos_th, double neg_th, double rem_th, std::uint32_t dec_th) {
                exp = normalize(value, pos_th, neg_th);
                integ = static_cast<std::uint32_t>(value);
                double rem = value - integ;
            }
        }  // namespace internal

        template <class Result, std::floating_point T>
        constexpr NumErr to_string(Result& result, T in, int radix = 10, bool upper = false) {
            if (radix != 10 && radix != 16) {
                return number::NumError::invalid;
            }
            if (in != in) {
                if (upper) {
                    result.push_back('N');
                    result.push_back('A');
                    result.push_back('N');
                }
                else {
                    result.push_back('n');
                    result.push_back('a');
                    result.push_back('n');
                }
                return true;
            }
            if (in < 0.0) {
                result.push_back('-');
            }
        }

    }  // namespace number
}  // namespace utils
