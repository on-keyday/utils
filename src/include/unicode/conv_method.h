/*license*/

// conv_method - utf conversion method
#pragma once

#include "../core/sequencer.h"
#include <minibuffer.h>
#include "internal/conv_method_helper.h"

namespace utils {
    namespace utf {

        template <class C>
        constexpr bool is_utf16_high_surrogate(C c) {
            return 0xD800 <= c && c <= 0xDC00;
        }

        template <class C>
        constexpr bool is_utf16_low_surrogate(C c) {
            return 0xDC00 <= c && c < 0xE000;
        }

        template <class T, class C = char32_t>
        constexpr bool utf8_to_utf32(Sequencer<T>& input, C& output) {
            using unsigned_t = std::make_unsigned_t<Sequencer<T>::char_type>;
            if (input.eos()) {
                return false;
            }
            auto ofs = [&](size_t ofs) {
                return static_cast<unsigned_t>(input.current(ofs));
            };
            auto in = ofs(0);
            if (in > 0xff) {
                return false;
            }
            if (in < 0x80) {
                input.consume();
                output = static_cast<C>(in);
                return true;
            }
            auto make = [&](auto len) {
                internal::make_utf32_from_utf8(len, input.buf, output, input.rptr);
            };
            auto test_mask = [&](size_t offset, std::uint8_t maskkind) {
                if (maskkind == 0) {
                    return true;
                }
                return ofs(offset) & internal::utf8bits(maskkind);
            };
            auto make_with_check = [&](size_t len, auto test1, auto test2) {
                if (input.remain() < len) {
                    return false;
                }
                if (!test_mask(0, test1) || !test_mask(1, test2)) {
                    return false;
                }
                make();
                input.consume(len);
                return true;
            };
            if (internal::is_mask_of<2>(in)) {
                return make_with_check(2, 5, 0);
            }
            else if (internal::is_mask_of<3>(in)) {
                return make_with_check(3, 6, 7);
            }
            else if (internal::is_mask_of<4>(in)) {
                return make_with_check(4, 8, 9);
            }
            return false;
        }

        template <class T, class C = char32_t>
        constexpr bool utf16_to_utf32(Sequencer<T>& input, C& output) {
            using unsigned_t = std::make_unsigned_t<Sequencer<T>::char_type>;
            if (input.eos()) {
                return false;
            }
            auto ofs = [&](size_t ofs) {
                return static_cast<unsigned_t>(input.current(ofs));
            };
            auto in = ofs(0);
            if (in > 0xffff) {
                return false;
            }
            if (is_utf16_high_surrogate(in)) {
                if (input.remain() < 2) {
                    return false;
                }
                auto sec = ofs(1);
                if (!is_utf16_low_surrogate(sec)) {
                    return false;
                }
                output = internal::make_surrogate_char<C>(in, sec);
                input.consume(2);
            }
            else {
                output = static_cast<C>(in);
                input.consume();
            }
            return true;
        }
    }  // namespace utf
}  // namespace utils
