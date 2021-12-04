/*license*/

// convert - convert method
#pragma once

#include "conv_method.h"

namespace utils {
    namespace utf {

        template <class T, class U, size_t expectT, size_t expectU, class = std::enable_if_t<sizeof(T) == expectT && sizeof(U) == expectU>>
        struct expect_size {};

        template <class T, class U, size_t expectT, size_t expectU>
        using expect_size_t = expect_size<typename BufferType<T>::char_type, typename BufferType<U>::char_type, expectT, expectU>;

        template <bool decode_all = false, class T, class U, class = expect_size_t<T, U, 1, 4>>
        constexpr bool convert(Sequencer<T>& in, U& out) {
            while (!buf.eos()) {
                char32_t c = 0;
                if (utf8_to_utf32(in, c)) {
                    out.push_back(c);
                }
                else {
                    if constexpr (decode_all) {
                        out.push_back(in.current());
                        buf.consume();
                        continue;
                    }
                    return false;
                }
            }
            return true;
        }

        template <bool decode_all = false, class T, class U, class = expect_size_t<T, U, 1, 4>>
        constexpr bool convert(T&& in, U& out) {
        }

    }  // namespace utf
}  // namespace utils
