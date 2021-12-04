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
        bool convert(T&& in, U& out) {
            Sequencer<typename BufferType<T&>::type> buf;
            while (!buf.eos()) {
                char32_t c = 0;
                if (utf8_to_utf32(buf, c)) {
                    out.push_back(c);
                }
                else {
                    if constexpr (decode_all) {
                        out.push_back(buf.current());
                        buf.consume();
                        continue;
                    }
                    return false;
                }
            }
            return true;
        }

    }  // namespace utf
}  // namespace utils
