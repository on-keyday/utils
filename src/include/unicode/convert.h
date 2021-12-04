/*license*/

// convert - generic function for convert utf
#pragma once

#include "conv_method.h"

namespace utils {
    namespace utf {

        template <class T, class U, size_t expectT, size_t expectU, class = std::enable_if_t<sizeof(T) == expectT && sizeof(U) == expectU>>
        struct expect_size {};

        template <class T, class U, size_t expectT, size_t expectU>
        using expect_size_t = expect_size<typename BufferType<T>::char_type, typename BufferType<U>::char_type, expectT, expectU>;

#define DEFINE_CONVERT_WITH_UTF32(FROM, TOMETHOD, FROMMETHOD)                                  \
    template <bool decode_all = false, class T, class U, class = expect_size_t<T, U, FROM, 4>> \
    constexpr bool convert(Sequencer<T>& in, U& out) {                                         \
        while (!in.eos()) {                                                                    \
            char32_t c = 0;                                                                    \
            if (TOMETHOD(in, c)) {                                                             \
                out.push_back(c);                                                              \
            }                                                                                  \
            else {                                                                             \
                if constexpr (decode_all) {                                                    \
                    out.push_back(in.current());                                               \
                    buf.consume();                                                             \
                    continue;                                                                  \
                }                                                                              \
                return false;                                                                  \
            }                                                                                  \
        }                                                                                      \
        return true;                                                                           \
    }                                                                                          \
                                                                                               \
    template <bool decode_all = false, class T, class U, class = expect_size_t<T, U, 4, FROM>> \
    constexpr bool convert(Sequencer<T>& in, U& out) {                                         \
        while (!in.eos()) {                                                                    \
            if (!FROMMETHOD(in.current(), out) && !decode_all) {                               \
                return false;                                                                  \
            }                                                                                  \
            in.consume();                                                                      \
        }                                                                                      \
        return true;                                                                           \
    }

        DEFINE_CONVERT_WITH_UTF32(1, utf8_to_utf32, utf32_to_utf8)
        DEFINE_CONVERT_WITH_UTF32(2, utf16_to_utf32, utf32_to_utf16)

        template <bool decode_all = false, class T, class U, class = expect_size_t<T, U, 2, 1>>
        constexpr bool convert(Sequencer<T>& in, U& out) {
            while (!in.eos()) {
                if (!utf16_to_utf8(in, out)) {
                    if (decode_all) {
                        out.push_back(in.current());
                        in.consume();
                        continue;
                    }
                    return false;
                }
            }
        }

        template <bool decode_all = false, class T, class U, class = expect_size_t<T, U, 1, 2>>
        constexpr bool convert() {
            while (!in.eos()) {
                if (!utf8_to_utf16(in, out)) {
                    if (decode_all) {
                        out.push_back(in.current());
                        in.consume();
                        continue;
                    }
                    return false;
                }
            }
            return true;
        }

        template <bool decode_all = false, class T, class U>
        constexpr bool convert(T&& in, U& out) {
            Sequencer<typename BufferType<T&>::type> seq(in);
            return convert<decode_all>(seq, out);
        }

    }  // namespace utf
}  // namespace utils
