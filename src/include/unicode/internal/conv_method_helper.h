/*license*/

// conv_method_helper - internal conv method
#pragma once
#include <cstdint>

#include "../../core/sequencer.h"

namespace utils {
    namespace utf {
        namespace internal {
            constexpr std::uint8_t utf8bits(std::uint8_t i) {
                const std::uint8_t maskbits[] = {
                    //first byte mask
                    0b10000000,
                    0b11000000,
                    0b11100000,
                    0b11110000,
                    0b11111000,

                    //i>=5,second byte later mask (safety)
                    0b00011110,              //two byte must
                    0b00001111, 0b00100000,  //three byte must
                    0b00000111, 0b00110000,  //four byte must
                };
                return i < sizeof(maskbits) ? maskbits[i] : 0;
            }

            template <std::uint8_t mask, class C>
            constexpr bool is_mask_of(C in) {
                return (utf8bits(mask) & in) == utf8bits(mask - 1);
            }

            template <class T, class C = char32_t>
            constexpr void make_utf32_from_utf8(size_t len, T& input, C& output, size_t offset) {
                using unsigned_t = std::make_unsigned_t<Sequencer<T>::char_type>;
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

        }  // namespace internal
    }      // namespace utf
}  // namespace utils