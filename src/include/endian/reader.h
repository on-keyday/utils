/*license*/

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
        };
    }  // namespace endian
}  // namespace utils
