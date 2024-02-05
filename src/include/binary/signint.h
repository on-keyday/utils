/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <binary/int128.h>
#include <type_traits>
#include <core/byte.h>
#include <cstdint>

namespace futils::binary {

    // make t signed integer type
    template <class T>
    constexpr auto sign(T t) {
        if constexpr (std::is_same_v<T, int128_t> ||
                      std::is_same_v<T, uint128_t>) {
            return int128_t(t);
        }
        else {
            return std::make_signed_t<T>(t);
        }
    }

    template <class T>
    using sign_t = decltype(sign(T{}));

    // make t unsigned integer type
    template <class T>
    constexpr auto uns(T t) {
        if constexpr (std::is_same_v<T, int128_t> ||
                      std::is_same_v<T, uint128_t>) {
            return uint128_t(t);
        }
        else {
            return std::make_unsigned_t<T>(t);
        }
    }

    template <class T>
    using uns_t = decltype(uns(T{}));

    template <class T>
    constexpr auto indexed_mask() {
        return [](auto i) {
            return uns(T(1)) << (sizeof(T) * bit_per_byte - 1 - i);
        };
    }

    template <class T, T value>
    constexpr auto decide_uint_type_by_value() {
        if constexpr (value <= 0xFF) {
            return std::uint8_t();
        }
        else if constexpr (value <= 0xFFFF) {
            return std::uint16_t();
        }
        else if constexpr (value <= 0xFFFFFFFF) {
            return std::uint32_t();
        }
        else if constexpr (value <= 0xFFFFFFFFFFFFFFFF) {
            return std::uint64_t();
        }
#ifdef FUTILS_BINARY_SUPPORT_INT128
        else {
            return uint128_t();
        }
#endif
    }

    template <byte value>
    constexpr auto decide_uint_type_by_byte_len() {
        if constexpr (value == 1) {
            return std::uint8_t();
        }
        else if constexpr (value == 2) {
            return std::uint16_t();
        }
        else if constexpr (value == 4) {
            return std::uint32_t();
        }
        else if constexpr (value == 8) {
            return std::uint64_t();
        }
#ifdef FUTILS_BINARY_SUPPORT_INT128
        else if constexpr (value == 16) {
            return uint128_t();
        }
#endif
    }

    template <class T, T value>
    using value_max_uint_t = decltype(decide_uint_type_by_value<uns_t<T>, uns_t<T>(value)>());

    template <byte l>
    using n_byte_uint_t = decltype(decide_uint_type_by_byte_len<l>());

    template <byte l>
    using n_bit_uint_t = n_byte_uint_t<(l >> 3)>;

    template <byte l>
    using n_byte_int_t = sign_t<n_byte_uint_t<l>>;

    template <byte l>
    using n_bit_int_t = n_byte_int_t<(l >> 3)>;

}  // namespace futils::binary
