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

            template <class T, size_t size = sizeof(T), size_t offset = 0, bool filldefault = false>
            constexpr size_t read(T& t, char fill = 0) {
                static_assert(size <= sizeof(T), "too large read size");
                static_assert(offset < size, "too large offset");
                if (seq.remain() < size - offset) {
                    if constexpr (!filldefault) {
                        return ~0;
                    }
                }
                char redbuf[sizeof(T)] = {0};
                T* ptr = reinterpret_cast<T*>(redbuf);
                size_t i = offset;
                size_t ret = 0;
                for (; i < size && !seq.eos(); i++) {
                    redbuf[i] = seq.current();
                    seq.consume();
                    ret++;
                }
                for (; i < size; i++) {
                    redbuf[i] = fill;
                }
                t = *ptr;
                return i;
            }

#define DEFINE_READ(FUNC, SUFFIX)                                                            \
    template <class T, size_t size = sizeof(T), size_t offset = 0, bool filldefault = false> \
    constexpr size_t read_##SUFFIX(T& t, char fill = 0) {                                    \
        auto ret = read<T, size, offset, filldefault>(t, fill);                              \
        if (ret == ~0) {                                                                     \
            return false;                                                                    \
        }                                                                                    \
        t = FUNC(&t);                                                                        \
        return ret;                                                                          \
    }
            DEFINE_READ(from_big, big)
            DEFINE_READ(from_little, little)
            DEFINE_READ(from_network, ntoh)

#undef DEFINE_READ
#define DEFINE_READ_SEQ(FUNC, SUFFIX)                                                                      \
    template <class T, class Result, size_t size = sizeof(T), size_t offset = 0, bool filldefault = false> \
    constexpr size_t read_##SUFFIX(Result& buf, size_t toread, char fill = 0) {                            \
        if (seq.remain() < size * toread) {                                                                \
            if constexpr (!filldefault) {                                                                  \
                return false;                                                                              \
            }                                                                                              \
        }                                                                                                  \
        size_t ret = 0;                                                                                    \
        for (size_t i = 0; i < toread; i++) {                                                              \
            T t;                                                                                           \
            ret += FUNC<T, size, offset, filldefault>(t, fill);                                            \
            buf.push_back(t);                                                                              \
        }                                                                                                  \
        return ret;                                                                                        \
    }

            DEFINE_READ_SEQ(read, seq)
            DEFINE_READ_SEQ(read_big, seq_big)
            DEFINE_READ_SEQ(read_little, seq_little)
            DEFINE_READ_SEQ(read_ntoh, seq_ntoh)

#undef DEFINE_READ_SEQ
        };  // namespace utils
    }       // namespace endian
}  // namespace utils
