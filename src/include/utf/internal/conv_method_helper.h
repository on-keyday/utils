/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// conv_method_helper - internal conv method
#pragma once
#include <cstdint>

#include "../../core/sequencer.h"

namespace utils {
    namespace utf {
        namespace internal {
            constexpr std::uint8_t utf8bits(std::uint8_t i) {
                const std::uint8_t maskbits[] = {
                    // first byte mask
                    0b10000000,
                    0b11000000,
                    0b11100000,
                    0b11110000,
                    0b11111000,

                    // i>=5,second byte later mask (safety)
                    0b00011110,              // two byte must
                    0b00001111, 0b00100000,  // three byte must
                    0b00000111, 0b00110000,  // four byte must
                };
                return i < sizeof(maskbits) ? maskbits[i] : 0;
            }

            template <class T, class C = char32_t>
            constexpr void make_utf32_from_utf8(size_t len, T& input, C& output, size_t offset) {
                using unsigned_t = std::make_unsigned_t<typename Sequencer<T>::char_type>;
                C ret = 0;
                for (int i = 0; i < len; i++) {
                    auto mul = (len - i - 1);
                    auto shift = 6 * mul;
                    unsigned char masking = 0;
                    if (i == 0) {
                        masking = input[offset] & ~utf8bits(len - 1);
                    }
                    else {
                        masking = input[offset + i] & ~utf8bits(1);
                    }
                    ret |= masking << shift;
                }
                output = ret;
            }

            template <class T, class C>
            constexpr C make_surrogate_char(T first, T second) {
                return 0x10000 + (first - 0xD800) * 0x400 + (second - 0xDC00);
            }

            template <size_t len, class T, class C>
            constexpr void make_utf8_from_utf32(C input, T& output) {
                constexpr std::uint8_t mask = static_cast<std::uint8_t>(~utf8bits(1));
                for (auto i = 0; i < len; i++) {
                    auto mul = (len - 1 - i);
                    auto shift = 6 * mul;
                    std::uint8_t abyte = 0, shiftC = static_cast<std::uint8_t>(input >> shift);
                    if (i == 0) {
                        abyte = utf8bits(len - 1) | (shiftC & mask);
                    }
                    else {
                        abyte = utf8bits(0) | (shiftC & mask);
                    }
                    output.push_back(abyte);
                }
            }

        }  // namespace internal

        template <std::uint8_t mask, class C>
        constexpr bool is_mask_of(C in) {
            static_assert(mask >= 1 && mask <= 4, "unexpected mask value");
            return (internal::utf8bits(mask) & in) == internal::utf8bits(mask - 1);
        }
    }  // namespace utf
}  // namespace utils
