/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "utf32.h"
#include "u8u16.h"

namespace utils::utf {

    template <class To, UTFSize<1> From>
    constexpr std::tuple<size_t, size_t, bool> utf8_to_utf32(To* to, size_t to_size, const From* from, size_t from_size, bool be = true) {
        const auto len = is_valid_utf8(from, from_size);
        if (len == 0) {
            return {0, 0, false};
        }
        if constexpr (UTFSize<To, 1>) {
            if (!to || to_size < 4) {
                return {4, len, false};
            }
            endian::write_into(to, compose_utf32_from_utf8(from, len), be);
            return {4, len, true};
        }
        else if constexpr (UTFSize<To, 4>) {
            if (!to || to_size < 1) {
                return {1, len, false};
            }
            to[0] = compose_utf8_from_utf32(from, len);
            return {1, len, true};
        }
        else {
            static_assert(UTFSize<To, 1> || UTFSize<To, 4>);
        }
    }

    template <UTFSize<1> To, class From>
    constexpr std::tuple<size_t, size_t, bool> utf32_to_utf8(To* to, size_t to_size, const From* from, size_t from_size, bool be = true) {
        if constexpr (UTFSize<To, 4>) {
            if (!is_valid_utf32(from, from_size)) {
                return false;
            }
            const auto len = expect_utf8_len(from[0]);
            if (len == 0) {
                return {0, 1, false};
            }
            return {len, 1, compose_utf8_from_utf32(from[0], to, len)};
        }
        else if constexpr (UTFSize<To, 1>) {
            auto [ok, first] = is_valid_utf32(from, from_size, be);
            if (!ok) {
                return false;
            }
            const auto len = expect_utf8_len(first);
            if (len == 0) {
                return {0, 4, false};
            }
            return {len, 4, compose_utf8_from_utf32(first, to, len)};
        }
        else {
            static_assert(UTFSize<From, 1> || UTFSize<From, 4>);
        }
    }

    template <UTFSize<1> To, class From>
    constexpr std::tuple<size_t, size_t, bool> utf16_to_utf8(To* to, size_t to_size, const From* from, size_t from_size, bool be = true) {
        if constexpr (UTFSize<From, 2>) {
            const auto [to_len, from_len] = is_utf16_to_utf8_convertible(to, to_size, from, from_size);
            if (to_len == 0) {
                return {0, 0, false};
            }
            return {to_len, from_len, compose_utf8_from_utf16(to, to_len, from, from_len)};
        }
        else if constexpr (UTFSize<From, 1>) {
            const auto [to_len, from_len] = is_utf16_to_utf8_convertible(to, to_size, from, from_size, be);
            if (to_len == 0) {
                return {0, 0, false};
            }
            return {to_len, from_len * 2, compose_utf8_from_utf16(to, to_len, from, from_len, be)};
        }
        else {
            static_assert(UTFSize<From, 1> || UTFSize<From, 2>);
        }
    }

    template <class To, UTFSize<1> From>
    constexpr std::tuple<size_t, size_t, bool> utf8_to_utf16(To* to, size_t to_size, const From* from, size_t from_size, bool be = true) {
        if constexpr (UTFSize<To, 2>) {
            const auto [to_len, from_len] = is_utf8_to_utf16_convertible(to, to_size, from, from_size);
            if (to_len == 0) {
                return {0, 0, false};
            }
            return {to_len, from_len, compose_utf16_from_utf8(to, to_len, from, from_len)};
        }
        else if constexpr (UTFSize<To, 1>) {
            const auto [to_len, from_len] = is_utf8_to_utf16_convertible(to, to_size, from, from_size, be);
            if (to_len == 0) {
                return {0, 0, false};
            }
            return {to_len * 2, from_len, compose_utf16_from_utf8(to, to_len, from, from_len, be)};
        }
        else {
            static_assert(UTFSize<From, 1> || UTFSize<From, 2>);
        }
    }

}  // namespace utils::utf
