/*license*/

// convert - generic function for convert utf
#pragma once

#include "conv_method.h"

namespace utils {
    namespace utf {

        template <class Buf, class T, class U, size_t expectT, size_t expectU, class = std::enable_if_t<sizeof(T) == expectT && sizeof(U) == expectU>>
        struct expect_size {
            using type = Buf;
        };

        template <class Buf, class T, class U, size_t expectT, size_t expectU>
        using expect_size_t = typename expect_size<Buf, typename BufferType<T>::char_type, typename BufferType<U>::char_type, expectT, expectU>::type;

#define DEFINE_CONVERT_WITH_UTF32(FROM, TOMETHOD, FROMMETHOD)                        \
    template <bool decode_all = false, class T, class U>                             \
    constexpr bool convert(Sequencer<T>& in, expect_size_t<U, T, U, FROM, 4>& out) { \
        while (!in.eos()) {                                                          \
            char32_t c = 0;                                                          \
            if (TOMETHOD(in, c)) {                                                   \
                out.push_back(c);                                                    \
            }                                                                        \
            else {                                                                   \
                if constexpr (decode_all) {                                          \
                    out.push_back(in.current());                                     \
                    in.consume();                                                    \
                    continue;                                                        \
                }                                                                    \
                return false;                                                        \
            }                                                                        \
        }                                                                            \
        return true;                                                                 \
    }                                                                                \
                                                                                     \
    template <bool decode_all = false, class T, class U>                             \
    constexpr bool convert(Sequencer<T>& in, expect_size_t<U, T, U, 4, FROM>& out) { \
        while (!in.eos()) {                                                          \
            if (!FROMMETHOD(in.current(), out) && !decode_all) {                     \
                return false;                                                        \
            }                                                                        \
            in.consume();                                                            \
        }                                                                            \
        return true;                                                                 \
    }

        DEFINE_CONVERT_WITH_UTF32(1, utf8_to_utf32, utf32_to_utf8)
        DEFINE_CONVERT_WITH_UTF32(2, utf16_to_utf32, utf32_to_utf16)

#define DEFINE_CONVERT_BETWEEN_UTF8_AND_UTF16(FROM, TO, METHOD)                       \
    template <bool decode_all = false, class T, class U>                              \
    constexpr bool convert(Sequencer<T>& in, expect_size_t<U, T, U, FROM, TO>& out) { \
        while (!in.eos()) {                                                           \
            if (!utf16_to_utf8(in, out)) {                                            \
                if (decode_all) {                                                     \
                    out.push_back(in.current());                                      \
                    in.consume();                                                     \
                    continue;                                                         \
                }                                                                     \
                return false;                                                         \
            }                                                                         \
        }                                                                             \
    }
        DEFINE_CONVERT_BETWEEN_UTF8_AND_UTF16(2, 1, utf16_to_utf8)
        DEFINE_CONVERT_BETWEEN_UTF8_AND_UTF16(1, 2, utf8_to_utf16)

#define DEFINE_CONVERT_BETWEEN_SAME_SIZE(SIZE)                                          \
    template <bool decode_all = false, class T, class U>                                \
    constexpr bool convert(Sequencer<T>& in, expect_size_t<U, T, U, SIZE, SIZE>& out) { \
        while (!in.eos()) {                                                             \
            out.push_back(in.current());                                                \
            in.consume();                                                               \
        }                                                                               \
        return true;                                                                    \
    }

        DEFINE_CONVERT_BETWEEN_SAME_SIZE(1)
        DEFINE_CONVERT_BETWEEN_SAME_SIZE(2)
        DEFINE_CONVERT_BETWEEN_SAME_SIZE(4)

        template <bool decode_all = false, class T, class U>
        constexpr bool convert(T&& in, U& out) {
            Sequencer<typename BufferType<T&>::type> seq(in);
            return convert<decode_all, typename BufferType<T&>::type, U>(seq, out);
        }

    }  // namespace utf
}  // namespace utils
