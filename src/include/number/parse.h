/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// parse - parse number
#pragma once

#include <cstdint>
#include <limits>

#include "../core/sequencer.h"

#include "../helper/pushbacker.h"

#include "char_range.h"

namespace utils {
    namespace number {

        namespace internal {

            template <class T>
            struct PushBackParserInt {
                T result = 0;
                bool overflow = false;
                int radix = 10;

                template <class C>
                constexpr void push_back(C in) {
                    if (overflow) {
                        return;
                    }
                    auto c = number_transform[in];
                    result += c;
                    constexpr T maxi = (std::numeric_limits<T>::max)();
                    if (!(result <= maxi / radix)) {
                        overflow = true;
                        return;
                    }
                    result *= radix;
                }
            };

            // experimental
            template <class T>
            struct PushBackParserFloat {
                int radix;
                T result1 = 0;
                T result2 = 0;
                T divrad = 1;
                T exp = 0;
                int plus = 0;
                bool afterdot = false;
                bool has_exp = false;
                bool minus_exp = false;
                template <class C>
                constexpr void push_back(C in) {
                    if (in == '.') {
                        afterdot = true;
                        // fallthrough (explicit not returning)
                    }
                    else if (in == 'p' || in == 'P') {
                        has_exp = true;
                        return;
                    }
                    else if (radix == 10 && (in == 'e' || in == 'E')) {
                        has_exp = true;
                        return;
                    }
                    else if (in == '+' || in == '-') {
                        minus_exp = in == '-';
                        return;
                    }
                    auto c = number_transform[in];
                    if (!afterdot) {
                        result1 += c;
                        result1 *= radix;
                    }
                    else if (!has_exp) {
                        result2 += c;
                        result2 *= radix;
                        divrad *= radix;
                        plus = 1;  // XXX: i don't know why this is works
                    }
                    else {
                        exp += c;
                        exp *= 10;
                    }
                }
            };

            // copied from other
            template <class T>
            constexpr T spow(T base, T exp) noexcept {
                return exp <= 0   ? 1
                       : exp == 1 ? base
                                  : base * spow(base, exp - 1);
            }
        }  // namespace internal

        template <class Result, class T>
        constexpr NumErr read_number(Result& result, Sequencer<T>& seq, int radix = 10, bool* is_float = nullptr) {
            if (radix < 2 || radix > 36) {
                return NumError::invalid;
            }
            bool zerosize = true;
            bool first = true;
            bool dot = false;
            bool exp = false;
            while (!seq.eos()) {
                auto e = seq.current();
                if (!is_in_byte_range(e)) {
                    break;
                }
                if (is_float) {
                    if (radix == 10 || radix == 16) {
                        if (!dot && e == '.') {
                            dot = true;
                            result.push_back('.');
                            seq.consume();
                            continue;
                        }
                        if (!exp &&
                            ((radix == 10 && (e == 'e' || e == 'E')) ||
                             (radix == 16 && (e == 'p' || e == 'P')))) {
                            if (first) {
                                return NumError::invalid;
                            }
                            dot = true;
                            exp = true;
                            result.push_back(e);
                            seq.consume();
                            if (seq.current() == '+' || seq.current() == '-') {
                                result.push_back(seq.current());
                                seq.consume();
                            }
                            radix = 10;
                            first = true;
                            continue;
                        }
                    }
                }
                auto n = number_transform[e];
                if (n < 0 || n >= radix) {
                    break;
                }
                result.push_back(e);
                seq.consume();
                first = false;
                zerosize = false;
            }
            if (first) {
                if (zerosize) {
                    return NumError::not_match;
                }
                return NumError::invalid;
            }
            if (is_float) {
                *is_float = dot || exp;
            }
            return true;
        }

        template <class String>
        constexpr NumErr is_number(String&& v, int radix = 10, size_t offset = 0, bool* is_float = nullptr, bool expect_eof = true) {
            Sequencer<buffer_t<String&>> seq(v);
            seq.seek(offset);
            auto e = read_number(helper::nop, seq, radix, is_float);
            if (!e) {
                return e;
            }
            if (expect_eof) {
                if (!seq.eos()) {
                    return NumError::not_eof;
                }
            }
            return true;
        }

        template <class String>
        constexpr NumErr is_float_number(String&& v, int radix = 10, size_t offset = 0, bool expect_eof = true) {
            if (radix != 10 && radix != 16) {
                return false;
            }
            bool is_float = false;
            auto e = is_number(v, radix, offset, &is_float, expect_eof);
            if (!e) {
                return e;
            }
            return is_float;
        }

        template <class String>
        constexpr NumErr is_integer(String&& v, int radix = 10, size_t offset = 0, bool expect_eof = true) {
            return is_number(v, radix, offset, nullptr, expect_eof);
        }

        template <class T, class U>
        constexpr NumErr parse_integer(Sequencer<T>& seq, U& result, int radix = 10) {
            internal::PushBackParserInt<U> parser;
            parser.radix = radix;
            bool minus = false;
            if (seq.current() == '+') {
                seq.consume();
            }
            else if (std::is_signed_v<U> && seq.current() == '-') {
                seq.consume();
                minus = true;
            }
            auto err = read_number(parser, seq, radix);
            if (!err) {
                return err;
            }
            if (parser.overflow) {
                return NumError::overflow;
            }
            result = parser.result / radix;
            if (minus) {
                result = -result;
            }
            return true;
        }

        template <class String, class T>
        constexpr NumErr parse_integer(String&& v, T& result, int radix = 10, size_t offset = 0, bool expect_eof = true) {
            Sequencer<buffer_t<String&>> seq(v);
            seq.seek(offset);
            T tmpres = 0;
            auto e = parse_integer(seq, tmpres, radix);
            if (!e) {
                return e;
            }
            if (expect_eof) {
                if (!seq.eos()) {
                    return NumError::not_eof;
                }
            }
            result = tmpres;
            return true;
        }

        // experimental
        template <class T, class U>
        constexpr NumErr parse_float(Sequencer<T>& seq, U& result, int radix = 10) {
            static_assert(std::is_floating_point_v<U>, "expect floating point type");
            if (radix != 10 && radix != 16) {
                return NumError::invalid;
            }
            bool minus = false;
            if (seq.current() == '+') {
                seq.consume();
            }
            else if (std::is_signed_v<U> && seq.current() == '-') {
                seq.consume();
                minus = true;
            }
            internal::PushBackParserFloat<U> parser;
            parser.radix = radix;
            bool is_flaot = false;
            auto e = read_number(parser, seq, radix, &is_flaot);
            if (!e) {
                return e;
            }
            result = parser.result1 / radix + parser.result2 / parser.divrad + parser.plus;
            if (parser.has_exp) {
                result = internal::spow(result, parser.exp / 10);
                if (parser.minus_exp) {
                    result = 1 / result;
                }
            }
            if (minus) {
                result = -result;
            }
            return true;
        }

        template <class String, class T>
        constexpr NumErr parse_float(String&& v, T& result, int radix = 10, size_t offset = 0, bool expect_eof = true) {
            Sequencer<buffer_t<String&>> seq(v);
            seq.seek(offset);
            T tmpres = 0;
            auto e = parse_float(seq, tmpres, radix);
            if (!e) {
                return e;
            }
            if (expect_eof) {
                if (!seq.eos()) {
                    return NumError::not_eof;
                }
            }
            result = tmpres;
            return true;
        }

    }  // namespace number
}  // namespace utils
