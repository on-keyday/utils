/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// writer - write binary following the endian rule
#pragma once

#include "endian.h"
#include <utility>
#include <cstddef>

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

#undef DEFINE_WRITE
#define DEFINE_WRITE_SEQ(FUNC, SUFFIX)                                                                                                                                                \
    template <class T, size_t size = sizeof(decltype(*std::declval<T>().begin())), size_t offset = 0>                                                                                 \
    constexpr void write_##SUFFIX(const T& seq) {                                                                                                                                     \
        for (auto& s : seq) {                                                                                                                                                         \
            FUNC<std::remove_cvref_t<decltype(s)>, size, offset>(s);                                                                                                                  \
        }                                                                                                                                                                             \
    }                                                                                                                                                                                 \
                                                                                                                                                                                      \
    template <class Begin, class End, class T = std::remove_cvref_t<decltype(*std::declval<Begin>())>, size_t size = sizeof(decltype(*std::declval<T>().begin())), size_t offset = 0> \
    constexpr void write_##SUFFIX(Begin begin, End end) {                                                                                                                             \
        for (auto i = begin; i != end; i++) {                                                                                                                                         \
            FUNC<std::remove_cvref_t<decltype(*i)>, size, offset>(*i);                                                                                                                \
        }                                                                                                                                                                             \
    }
            DEFINE_WRITE_SEQ(write, seq)
            DEFINE_WRITE_SEQ(write_big, seq_big)
            DEFINE_WRITE_SEQ(write_little, seq_little)
            DEFINE_WRITE_SEQ(write_hton, seq_hton)
        };
#undef DEFINE_WRITE_SEQ

    }  // namespace endian
}  // namespace utils
