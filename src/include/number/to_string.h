/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// to_string - number to string
#pragma once

#include <limits>
#include <concepts>

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

        // reference implementation
        // https://blog.benoitblanchon.fr/lightweight-float-to-string/

        namespace internal {
#define VALUE(v)          \
    if (value >= 1e##v) { \
        value /= 1e##v;   \
        exp += v;         \
    }
            constexpr int normalize(double& value, const double positive, const double negative) {
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
#define VALUE(v1, v2)      \
    if (value < 1e-##v1) { \
        value *= 1e##v2;   \
        exp -= v2;         \
    }

                if (value > 0 && value <= negative) {
                    VALUE(255, 256)
                    VALUE(127, 128)
                    VALUE(63, 64)
                    VALUE(31, 32)
                    VALUE(15, 16)
                    VALUE(7, 8)
                    VALUE(3, 4)
                    VALUE(1, 2)
                    VALUE(0, 1)
                }
#undef VALUE
                return exp;
            }

            template <class InT = std::uint32_t, class Exp = std::int16_t>
            constexpr void split_float(double value, InT& integ, InT& decimal, Exp& exp,
                                       const double pos_th, const double neg_th, const double rem_th,
                                       const InT dec_th, const double round_th) {
                exp = normalize(value, pos_th, neg_th);
                integ = static_cast<InT>(value);
                double rem = value - integ;
                rem *= rem_th;
                decimal = static_cast<InT>(rem);
                rem -= decimal;
                if (rem >= round_th) {
                    decimal++;
                    if (decimal >= dec_th) {
                        decimal = 0;
                        integ++;
                        if (exp != 0 && integ >= 10) {
                            exp++;
                            integ = 1;
                        }
                    }
                }
            }

            template <class Result, class InT>
            constexpr number::NumErr write_decimal(Result& result, InT value, int width, int radix, bool upper) {
                while (value % radix == 0 && width > 0) {
                    value /= radix;
                    width--;
                }
                result.push_back('.');
                return to_string(result, value, radix, upper);
            }
        }  // namespace internal

        template <class Result, std::floating_point T, class InT = std::uint32_t, class Exp = std::int16_t>
        constexpr NumErr to_string(Result& result, T in, int radix = 10, bool upper = false, int decdigit = 9) {
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
            InT integ;
            InT decimal;
            Exp exp;
            auto p = internal::spow(10, decdigit);
            internal::split_float<InT, Exp>(in, integ, decimal, exp,
                                            1e7, 1e-5, p, p, 0.5);
            auto err = to_string(result, integ, radix, upper);
            if (!err) {
                return err;
            }
            if (decimal) {
                err = internal::write_decimal(result, decimal, decdigit, radix, upper);
                if (!err) {
                    return err;
                }
            }
            if (exp) {
                if (radix == 16) {
                    result.push_back(upper ? 'P' : 'p');
                }
                else {
                    result.push_back(upper ? 'E' : 'e');
                }
                if (exp < 0) {
                    result.push_back('-');
                }
                err = to_string(result, exp < 0 ? -exp : exp, radix, upper);
                if (!err) {
                    return err;
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

        template <class Result, std::floating_point T, class InT = std::uint32_t, class Exp = std::int16_t>
        constexpr Result to_string(T in, int radix = 10, bool upper = false, int decdigit = 9) {
            Result result;
            to_string(result, in, radix, upper, decdigit);
            return result;
        }

    }  // namespace number
}  // namespace utils
