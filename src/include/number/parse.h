/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
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
                    if (result) {
                        constexpr T maxi = (std::numeric_limits<T>::max)();
                        if (!(result <= maxi / radix)) {
                            overflow = true;
                            return;
                        }
                        result *= radix;
                    }
                    auto c = number_transform[int(in)];
                    result += c;
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
                    auto c = number_transform[int(in)];
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

            struct ReadConfig {
                static constexpr char dot = '.';
                static constexpr bool accept_exp = true;
                constexpr static bool ignore(auto&& c) {
                    return false;
                }
                static constexpr size_t offset = 0;
                static constexpr bool expect_eof = true;
                static constexpr bool allow_zero_prefixed = true;
            };

        }  // namespace internal

        constexpr auto default_num_ignore() {
            return [](auto&& a) {
                return false;
            };
        }

        template <class Ignore>
        struct NumConfig {
            Ignore ignore;
            char dot = '.';
            bool accept_exp = true;
            size_t offset = 0;
            bool expect_eof = true;
            bool allow_zero_prefixed = true;
        };

        template <class Ignore>
        NumConfig(Ignore) -> NumConfig<Ignore>;

        struct OffsetConfig {
            static constexpr char dot = '.';
            static constexpr bool accept_exp = true;
            constexpr static bool ignore(auto&& c) {
                return false;
            }
            size_t offset = 0;
            static constexpr bool expect_eof = true;
            static constexpr bool allow_zero_prefixed = true;
        };

        template <class Result, class T, class Config = internal::ReadConfig>
        constexpr NumErr read_number(Result& result, Sequencer<T>& seq, int radix = 10, bool* is_float = nullptr, Config&& config = Config{}) {
            if (!acceptable_radix(radix)) {
                return NumError::invalid;
            }
            bool zero_prefix = false;
            size_t count = 0;
            // bool zerosize = true;
            bool on_err = true;
            bool dot = false;
            bool exp = false;
            while (!seq.eos()) {
                auto e = seq.current();
                if (!is_in_byte_range(e)) {
                    if (config.ignore(e)) {
                        continue;
                    }
                    break;
                }
                if (is_float) {
                    if (radix == 10 || radix == 16) {
                        if (!dot && e == config.dot) {
                            dot = true;
                            result.push_back(config.dot);
                            seq.consume();
                            continue;
                        }
                        if (config.accept_exp) {
                            if (!exp &&
                                ((radix == 10 && (e == 'e' || e == 'E')) ||
                                 (radix == 16 && (e == 'p' || e == 'P')))) {
                                if (on_err) {
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
                                on_err = true;
                                continue;
                            }
                        }
                    }
                }
                auto n = number_transform[int(e)];
                if (n < 0 || n >= radix) {
                    if (config.ignore(e)) {
                        continue;
                    }
                    break;
                }
                if (!config.allow_zero_prefixed && e == '0' && count == 0) {
                    zero_prefix = true;
                }
                result.push_back(e);
                seq.consume();
                on_err = false;
                count++;
            }
            if (on_err) {
                if (count == 0) {
                    return NumError::not_match;
                }
                return NumError::invalid;
            }
            if (zero_prefix && count != 1 && !dot && !exp) {
                return NumError::invalid;
            }
            if (is_float) {
                *is_float = dot || exp;
            }
            return true;
        }

        template <class String, class Config = internal::ReadConfig>
        constexpr NumErr is_number(String&& v, int radix = 10, bool* is_float = nullptr, Config&& config = Config{}) {
            Sequencer<buffer_t<String&>> seq(v);
            seq.seek(config.offset);
            auto e = read_number(helper::nop, seq, radix, is_float, config);
            if (!e) {
                return e;
            }
            if (config.expect_eof) {
                if (!seq.eos()) {
                    return NumError::not_eof;
                }
            }
            return true;
        }

        template <class String, class Config = internal::ReadConfig>
        constexpr NumErr is_float_number(String&& v, int radix = 10, Config&& config = Config{}) {
            if (radix != 10 && radix != 16) {
                return false;
            }
            bool is_float = false;
            auto e = is_number(v, radix, &is_float, config);
            if (!e) {
                return e;
            }
            return is_float;
        }

        template <class String, class Config = internal::ReadConfig>
        constexpr NumErr is_integer(String&& v, int radix = 10, Config&& config = Config{}) {
            return is_number(v, radix, nullptr, config);
        }

        template <class T, class U, class Config = internal::ReadConfig>
        constexpr NumErr parse_integer(Sequencer<T>& seq, U& result, int radix = 10, Config&& config = Config{}) {
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
            auto err = read_number(parser, seq, radix, nullptr, config);
            if (!err) {
                return err;
            }
            if (parser.overflow) {
                return NumError::overflow;
            }
            result = parser.result;
            if (minus) {
                result = -result;
            }
            return true;
        }

        template <class String, class T, class Config = internal::ReadConfig>
        constexpr NumErr parse_integer(String&& v, T& result, int radix = 10, Config config = Config{}) {
            Sequencer<buffer_t<String&>> seq(v);
            seq.seek(config.offset);
            T tmpres = 0;
            auto e = parse_integer(seq, tmpres, radix, config);
            if (!e) {
                return e;
            }
            if (config.expect_eof) {
                if (!seq.eos()) {
                    return NumError::not_eof;
                }
            }
            result = tmpres;
            return true;
        }

        // experimental
        template <class T, class U, class Config = internal::ReadConfig>
        constexpr NumErr parse_float(Sequencer<T>& seq, U& result, int radix = 10, Config config = Config{}) {
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
            auto e = read_number(parser, seq, radix, &is_flaot, config);
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

        template <class String, class T, class Config = internal::ReadConfig>
        constexpr NumErr parse_float(String&& v, T& result, int radix = 10, Config config = Config{}) {
            Sequencer<buffer_t<String&>> seq(v);
            seq.seek(config.offset);
            T tmpres = 0;
            auto e = parse_float(seq, tmpres, radix, config);
            if (!e) {
                return e;
            }
            if (config.expect_eof) {
                if (!seq.eos()) {
                    return NumError::not_eof;
                }
            }
            result = tmpres;
            return true;
        }

        template <size_t limit, class In, class T, class Config = internal::ReadConfig>
        constexpr NumErr read_limited_int(Sequencer<In>& seq, T& t, int radix = 10, bool must = false, Config config = Config{}) {
            char num[limit + 1] = {0};
            size_t count = 0;
            while (!seq.eos() && is_radix_char(seq.current(), radix) && count < limit) {
                num[count] = seq.current();
                count++;
                seq.consume();
            }
            if (count == 0) {
                return NumError::invalid;
            }
            if (must) {
                if (count != limit) {
                    return NumError::invalid;
                }
            }
            return number::parse_integer(num, t, radix, config);
        }

    }  // namespace number
}  // namespace utils
