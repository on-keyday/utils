/*license*/

// convert_impl - implementation of convert()
#pragma once

#include "../conv_method.h"

namespace utils {
    namespace utf {
        namespace internal {
            template <class Buf, class T, class U, size_t expectT, size_t expectU, class = std::enable_if_t<sizeof(T) == expectT && sizeof(U) == expectU>>
            struct expect_size {
                using type = Buf;
            };

            template <class Buf, class T, class U, size_t expectT, size_t expectU>
            using expect_size_t = typename expect_size<Buf, typename BufferType<T>::char_type, typename BufferType<U>::char_type, expectT, expectU>::type;

#define DEFINE_CONVERT_WITH_UTF32(FROM, TOMETHOD, FROMMETHOD)                             \
    template <bool decode_all, class T, class U>                                          \
    constexpr bool convert_impl(Sequencer<T>& in, expect_size_t<U, T, U, FROM, 4>& out) { \
        char32_t c = 0;                                                                   \
        if (TOMETHOD(in, c)) {                                                            \
            out.push_back(c);                                                             \
        }                                                                                 \
        else {                                                                            \
            if constexpr (decode_all) {                                                   \
                out.push_back(in.current());                                              \
                in.consume();                                                             \
                return true;                                                              \
            }                                                                             \
            return false;                                                                 \
        }                                                                                 \
        return true;                                                                      \
    }                                                                                     \
                                                                                          \
    template <bool decode_all, class T, class U>                                          \
    constexpr bool convert_impl(Sequencer<T>& in, expect_size_t<U, T, U, 4, FROM>& out) { \
        if (!FROMMETHOD(in.current(), out) && !decode_all) {                              \
            return false;                                                                 \
        }                                                                                 \
        in.consume();                                                                     \
        return true;                                                                      \
    }

            DEFINE_CONVERT_WITH_UTF32(1, utf8_to_utf32, utf32_to_utf8)
            DEFINE_CONVERT_WITH_UTF32(2, utf16_to_utf32, utf32_to_utf16)

#define DEFINE_CONVERT_BETWEEN_UTF8_AND_UTF16(FROM, TO, METHOD)                            \
    template <bool decode_all, class T, class U>                                           \
    constexpr bool convert_impl(Sequencer<T>& in, expect_size_t<U, T, U, FROM, TO>& out) { \
        if (!METHOD(in, out)) {                                                            \
            if constexpr (decode_all) {                                                    \
                out.push_back(in.current());                                               \
                in.consume();                                                              \
                return true;                                                               \
            }                                                                              \
            return false;                                                                  \
        }                                                                                  \
        return true;                                                                       \
    }
            DEFINE_CONVERT_BETWEEN_UTF8_AND_UTF16(2, 1, utf16_to_utf8)
            DEFINE_CONVERT_BETWEEN_UTF8_AND_UTF16(1, 2, utf8_to_utf16)

#define DEFINE_CONVERT_BETWEEN_SAME_SIZE(SIZE)                                               \
    template <bool decode_all, class T, class U>                                             \
    constexpr bool convert_impl(Sequencer<T>& in, expect_size_t<U, T, U, SIZE, SIZE>& out) { \
        out.push_back(in.current());                                                         \
        in.consume();                                                                        \
        return true;                                                                         \
    }

            DEFINE_CONVERT_BETWEEN_SAME_SIZE(1)
            DEFINE_CONVERT_BETWEEN_SAME_SIZE(2)
            DEFINE_CONVERT_BETWEEN_SAME_SIZE(4)

        }  // namespace internal
    }      // namespace utf
}  // namespace utils
