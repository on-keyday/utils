/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// reader - read binary fallowing the endian rule
#pragma once

#include "endian.h"
#include "../core/sequencer.h"
#include <utility>
#include <type_traits>

namespace utils {
    namespace endian {
        template <class Buf>
        struct Reader {
            Sequencer<Buf> seq;

            template <class... Args>
            constexpr Reader(Args&&... args)
                : seq(std::forward<Args>(args)...) {}

            template <class T, size_t size = sizeof(T), size_t offset = 0>
            constexpr bool read(T& t) {
                static_assert(size <= sizeof(T), "too large read size");
                static_assert(offset < size, "too large offset");
                if (seq.remain() < size - offset) {
                    return false;
                }
                char redbuf[sizeof(T)] = {0};
                T* ptr = reinterpret_cast<T*>(redbuf);
                for (size_t i = offset; i < size; i++) {
                    redbuf[i] = seq.current();
                    seq.consume();
                }
                t = *ptr;
                return true;
            }

#define DEFINE_READ(FUNC, SUFFIX)                                  \
    template <class T, size_t size = sizeof(T), size_t offset = 0> \
    constexpr bool read_##SUFFIX(T& t) {                           \
        if (!read<T, size, offset>(t)) {                           \
            return false;                                          \
        }                                                          \
        t = FUNC(&t);                                              \
        return true;                                               \
    }
            DEFINE_READ(from_big, big)
            DEFINE_READ(from_little, little)
            DEFINE_READ(from_network, ntoh)

#undef DEFINE_READ
#define DEFINE_READ_SEQ(FUNC, SUFFIX)                                            \
    template <class T, class Result, size_t size = sizeof(T), size_t offset = 0> \
    constexpr bool read_##SUFFIX(Result& buf, size_t toread) {                   \
        if (seq.remain() < size * toread) {                                      \
            return false;                                                        \
        }                                                                        \
        for (size_t i = 0; i < toread; i++) {                                    \
            T t;                                                                 \
            FUNC<T, size, offset>(t);                                            \
            buf.push_back(t);                                                    \
        }                                                                        \
        return true;                                                             \
    }

            DEFINE_READ_SEQ(read, seq)
            DEFINE_READ_SEQ(read_big, seq_big)
            DEFINE_READ_SEQ(read_little, seq_little)
            DEFINE_READ_SEQ(read_ntoh, seq_ntoh)

#undef DEFINE_READ_SEQ
        };
    }  // namespace endian
}  // namespace utils
