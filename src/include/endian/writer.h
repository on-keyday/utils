/*license*/

// writer - write binary following the endian rule
#pragma once

#include "endian.h"
#include <utility>

namespace utils {
    namespace endian {

        template <class Buf>
        struct Writer {
            using raw_type = Buf;

            raw_type buf;

            template <class... Args>
            constexpr Writer(Args&&... args)
                : buf(std::forward<Args>(args)...) {}

            template <class T, size_t size = sizeof(T), size_t offset = 0>
            constexpr void write(const T& t) {
                static_assert(size <= sizeof(T), "size is too large");
                static_assert(offset < size, "offset is too large");
                const char* ptr = reinterpret_cast<const char*>(&t);
                for (auto i = offset; i < size; i++) {
                    buf.push_back(ptr[i]);
                }
            }

#define DEFINE_WRITE(FUNC, SUFFIX)                                 \
    template <class T, size_t size = sizeof(T), size_t offset = 0> \
    constexpr void write_##SUFFIX(const T& t) {                    \
        write<T, size, offset>(FUNC(&t));                          \
    }
            DEFINE_WRITE(to_big, big)
            DEFINE_WRITE(to_little, little)
            DEFINE_WRITE(to_network, hton)

#define DEFINE_WRITE_SEQ(FUNC, SUFFIX)                             \
    template <class T, size_t size = sizeof(T), size_t offset = 0> \
    constexpr void write_##SUFFIX(const T& seq) {                  \
        for (auto& s : seq) {                                      \
            FUNC<T, size, offset>(s);                              \
        }                                                          \
    }                                                              \
                                                                   \
    template <class Begin, class End>                              \
    constexpr void write_##SUFFIX(Begin begin, End end) {          \
        for (auto i = begin; i != end; i++) {                      \
            FUNC<T, size, offset>(*i);                             \
        }                                                          \
    }
            DEFINE_WRITE_SEQ(write, seq)
            DEFINE_WRITE_SEQ(write_big, seq_big)
            DEFINE_WRITE_SEQ(write_little, seq_little)
            DEFINE_WRITE_SEQ(write_hton, seq_hton)
        };
#undef DEFINE_WRITE_SEQ
    }  // namespace endian
}  // namespace utils
