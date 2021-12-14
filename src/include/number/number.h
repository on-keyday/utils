/*license*/

// parse - parse number
#pragma once

#include <cstdint>
#include <limits>

#include <charconv>

#include "../core/sequencer.h"

#include "../wrap/lite/enum.h"

namespace utils {
    namespace number {
        // clang-format off
        constexpr std::int8_t number_transform[] = {
        //   0   1   2   3   4   5   6   7   8   9   a   b   c   d   e   f
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0x00 - 0x0f
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0x10 - 0x1f
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0x20 - 0x2f
             0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1, // 0x30 - 0x3f
            -1, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, // 0x40 - 0x4f
            25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, -1, -1, -1, -1, -1, // 0x50 - 0x5f
            -1, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, // 0x60 - 0x6f
            25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, -1, -1, -1, -1, -1, // 0x70 - 0x7f
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0x80 - 0x8f
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0x90 - 0x9f
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0xa0 - 0xaf
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0xb0 - 0xbf
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0xc0 - 0xcf
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0xd0 - 0xdf
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0xe0 - 0xef
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0xf0 - 0xff
        };
        // clang-format on

        namespace internal {

            struct NopPushBacker {
                template <class T>
                constexpr void push_back(T&&) {}
            };

            template <class T>
            struct PushBackParserInt {
                bool overflow = false;
                int radix = 10;
                T result = 0;

                template <class C>
                constexpr void push_back(C in) {
                    if (overflow) {
                        return;
                    }
                    auto c = number_transform[in];
                    result += c;
                    constexpr T maxi = (std::numeric_limits<T>::max)();
                    if (!(result < maxi / radix)) {
                        overflow = true;
                        return;
                    }
                    result *= radix;
                }
            };

            // experimental
            template <class T>
            struct PushBackParserFloat {
                bool unsurpport = false;
                int radix;
                T result1 = 0;
                T result2 = 0;
                bool afterdot = false;
                template <class C>
                constexpr void push_back(C in) {
                    if (in == '.') {
                        afterdot = true;
                    }
                    else if (in == 'p' || in == 'P') {
                        unsurpport = true;
                        return;
                    }
                    else if (radix == 10 && (in == 'e' || in == 'E')) {
                        unsurpport = true;
                        return;
                    }
                    auto c = number_transform[in];
                    if (!afterdot) {
                        result1 += c;
                        result1 *= radix;
                    }
                    else {
                        result2 += c;
                        result2 *= radix;
                    }
                }
            };
        }  // namespace internal

        enum class NumError {
            none,
            not_match,
            invalid,
            overflow,
            not_eof,
        };

        using NumErr = wrap::EnumWrap<NumError, NumError::none, NumError::not_match>;

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
                if (e < 0 || e > 0xff) {
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
            internal::NopPushBacker nop;
            seq.seek(offset);
            auto e = read_number(nop, seq, radix, is_float);
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
        NumErr parse_float(Sequencer<T>& seq, U& result, int radix = 10) {
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
            if (parser.unsurpport) {
                return NumError::invalid;
            }
        }
    }  // namespace number
}  // namespace utils
