/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// conv_method - utf conversion method implementation
#pragma once

#include "../core/sequencer.h"
#include "minibuffer.h"
#include "internal/conv_method_helper.h"
#include "../wrap/light/enum.h"

namespace utils {
    namespace utf {

        enum class UTFError {
            none,
            length,
            invalid_first,
            illformed_sequence,
            out_of_range,
            unknown,
        };

        using UTFErr = wrap::EnumWrap<UTFError, UTFError::none, UTFError::unknown>;

        template <class C>
        constexpr bool is_utf16_high_surrogate(C c) {
            return 0xD800 <= c && c <= 0xDC00;
        }

        template <class C>
        constexpr bool is_utf16_low_surrogate(C c) {
            return 0xDC00 <= c && c < 0xE000;
        }

        template <class C>
        constexpr std::uint8_t expect_len(C in) {
            std::make_unsigned_t<C> c = in;
            if (c < 0 || c > 0xff) {
                return 0;
            }
            if (c >= 0 && c <= 0x7f) {
                return 1;
            }
            else if (is_mask_of<2>(c)) {
                return 2;
            }
            else if (is_mask_of<3>(c)) {
                return 3;
            }
            else if (is_mask_of<4>(c)) {
                return 4;
            }
            return 0;
        }

        constexpr bool is_valid_range(std::uint8_t len, std::uint8_t one, std::uint8_t two) {
            auto normal_two_range = [&] {
                return 0x80 <= two && two <= 0xBF;
            };
            switch (len) {
                case 1:
                    return true;
                case 2:
                    return 0xC2 <= one && one <= 0xDF &&
                           normal_two_range();
                case 3:
                    if (one == 0xE0) {
                        return 0xA0 <= two && two <= 0xBF;
                    }
                    else if (one == 0xED) {
                        return 0x80 <= two && two <= 0x9F;
                    }
                    return 0xE0 <= one && one <= 0xEF &&
                           normal_two_range();
                case 4:
                    if (one == 0xF0) {
                        return 0x90 <= two && two <= 0xBF;
                    }
                    else if (one == 0xF4) {
                        return 0x80 <= two && two <= 0x8F;
                    }
                    return 0xF0 <= one && one <= 0xF4 &&
                           normal_two_range();
                default:
                    return false;
            }
        }

        template <class T, class C = char32_t>
        constexpr UTFErr utf8_to_utf32(Sequencer<T>& input, C& output) {
            using unsigned_t = std::make_unsigned_t<typename Sequencer<T>::char_type>;
            if (input.eos()) {
                return UTFError::length;
            }
            auto ofs = [&](size_t ofs) {
                return static_cast<unsigned_t>(input.current(ofs));
            };
            auto in = ofs(0);
            auto len = expect_len(in);
            if (len == 0) {
                return UTFError::invalid_first;
            }
            if (len == 1) {
                input.consume();
                output = static_cast<C>(in);
                return true;
            }
            if (input.remain() < len) {
                return UTFError::length;
            }
            if (!is_valid_range(len, ofs(0), ofs(1))) {
                return UTFError::illformed_sequence;
            }
            internal::make_utf32_from_utf8(len, input.buf.buffer, output, input.rptr);
            input.consume(len);
            return true;
        }

        template <class T, class C = char32_t>
        constexpr UTFErr utf8_to_utf32(T&& input, C& output) {
            Sequencer<buffer_t<T&>> seq(input);
            return utf8_to_utf32(seq, output);
        }

        template <class T, class C = char32_t>
        constexpr UTFErr utf16_to_utf32(Sequencer<T>& input, C& output) {
            using unsigned_t = std::make_unsigned_t<typename Sequencer<T>::char_type>;
            if (input.eos()) {
                return UTFError::length;
            }
            auto ofs = [&](size_t ofs) {
                return static_cast<unsigned_t>(input.current(ofs));
            };
            auto in = ofs(0);
            if (in > 0xffff) {
                return UTFError::invalid_first;
            }
            if (is_utf16_high_surrogate(in)) {
                if (input.remain() < 2) {
                    return UTFError::length;
                }
                auto sec = ofs(1);
                if (!is_utf16_low_surrogate(sec)) {
                    return UTFError::illformed_sequence;
                }
                output = internal::make_surrogate_char<unsigned_t, C>(in, sec);
                input.consume(2);
            }
            else {
                output = static_cast<C>(in);
                input.consume();
            }
            return true;
        }

        template <class T, class C = char32_t>
        constexpr UTFErr utf16_to_utf32(T&& input, C& output) {
            Sequencer<buffer_t<T&>> seq(input);
            return utf16_to_utf32(seq, output);
        }

        template <class T, class C>
        constexpr UTFErr utf32_to_utf8(C input, T& output) {
            using unsigned_t = std::make_unsigned_t<C>;
            auto in = static_cast<unsigned_t>(input);
            if (in < 0x80) {
                output.push_back(in);
            }
            else if (in < 0x800) {
                internal::make_utf8_from_utf32<2>(in, output);
            }
            else if (in < 0x10000) {
                internal::make_utf8_from_utf32<3>(in, output);
            }
            else if (in < 0x110000) {
                internal::make_utf8_from_utf32<4>(in, output);
            }
            else {
                return UTFError::out_of_range;
            }
            return true;
        }

        template <class T, class C>
        constexpr UTFErr utf32_to_utf16(C input, T& output) {
            using unsigned_t = std::make_unsigned_t<C>;
            auto in = static_cast<unsigned_t>(input);
            if (in >= 0x110000) {
                return UTFError::out_of_range;
            }
            if (in > 0xffff) {
                auto first = char16_t((in - 0x10000) / 0x400 + 0xD800);
                auto second = char16_t((in - 0x10000) % 0x400 + 0xDC00);
                output.push_back(first);
                output.push_back(second);
            }
            else {
                output.push_back(in);
            }
            return true;
        }

        template <class T, class C>
        constexpr UTFErr utf8_to_utf16(T& input, C& output) {
            char32_t tmp = 0;
            if (auto ok = utf8_to_utf32(input, tmp); !ok) {
                return ok;
            }
            return utf32_to_utf16(tmp, output);
        }

        template <class T, class C>
        constexpr UTFErr utf16_to_utf8(T& input, C& output) {
            char32_t tmp = 0;
            if (auto ok = utf16_to_utf32(input, tmp); !ok) {
                return ok;
            }
            return utf32_to_utf8(tmp, output);
        }

    }  // namespace utf
}  // namespace utils
