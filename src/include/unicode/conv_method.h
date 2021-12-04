/*license*/

// conv_method - utf conversion method
#pragma once

#include "../core/sequencer.h"
#include <minibuffer.h>
#include "internal/conv_method_helper.h"

namespace utils {
    namespace utf {

        template <class T, class C = char32_t>
        constexpr bool utf8_to_utf32(Sequencer<T>& input, C& output) {
            using unsigned_t = std::make_unsigned_t<Sequencer<T>::char_type>;
            auto now = [&] {
                return static_cast<unsigned_t>(input.current());
            };
            auto in = now();
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
            if (is_mask_of<2>(in)) {
                if (input.remain() < 2) {
                    return false;
                }
                make();
                input.consume(2);
            }
        }
    }  // namespace utf
}  // namespace utils
