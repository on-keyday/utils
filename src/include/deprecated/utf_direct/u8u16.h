/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// u8u16 - utf8 and utf16 convertion
#pragma once
#include "utf8.h"
#include "utf16.h"

namespace utils::utf {
    constexpr size_t utf8_len_to_utf16_len(size_t len) noexcept {
        switch (len) {
            case 1:
                return 1;
            case 2:
                return 1;
            case 3:
                return 1;
            case 4:
                return 2;
            default:
                return 0;
        }
    }

    template <UTFSize<1> From, UTFSize<2> To>
    constexpr bool compose_utf16_from_utf8(To* to, size_t tolen, const From* code, size_t fromlen) noexcept {
        return compose_utf16_from_utf32(compose_utf32_from_utf8(code, fromlen), to, tolen);
    }

    template <UTFSize<1> From, UTFSize<1> To>
    constexpr bool compose_utf16_from_utf8(To* to, size_t tolen, const From* code, size_t fromlen, bool be) noexcept {
        return compose_utf16_from_utf32(compose_utf32_from_utf8(code, fromlen), to, tolen, be);
    }

    template <UTFSize<2> From, UTFSize<1> To>
    constexpr bool compose_utf8_from_utf16(To* to, size_t tolen, const From* code, size_t fromlen) noexcept {
        return compose_utf8_from_utf32(compose_utf32_from_utf16(code, fromlen), to, tolen);
    }

    template <UTFSize<1> From, UTFSize<1> To>
    constexpr bool compose_utf8_from_utf16(To* to, size_t tolen, const From* code, size_t fromlen, bool be) noexcept {
        return compose_utf8_from_utf32(compose_utf32_from_utf16(code, fromlen, be), to, tolen);
    }

    // returns (to,from)
    template <UTFSize<1> From, UTFSize<2> To>
    constexpr std::pair<size_t, size_t> is_utf8_to_utf16_convertible(To* to, size_t tosize, const From* code, size_t fromsize) {
        const auto hit = is_valid_utf8(code, fromsize);
        switch (hit) {
            case 0:
                return {0, 0};
            case 1:
            case 2:
            case 3:
                if (!to || tosize < 1) {
                    return {0, hit};
                }
                return {1, hit};
            case 4:
                if (!to || tosize < 2) {
                    return {0, hit};
                }
                return {2, 4};
            default:
                return {0, hit};
        }
    }

    // returns (to(utf16 count=byte/2),from)
    template <UTFSize<1> From, UTFSize<1> To>
    constexpr std::pair<size_t, size_t> is_utf8_to_utf16_convertible(To* to, size_t tosize, const From* code, size_t fromsize) {
        const auto hit = is_valid_utf8(code, fromsize);
        switch (hit) {
            case 0:
                return {0, 0};
            case 1:
            case 2:
            case 3:
                if (!to || tosize < 2) {
                    return {0, hit};
                }
                return {1, hit};
            case 4:
                if (!to || tosize < 4) {
                    return {0, hit};
                }
                return {2, 4};
            default:
                return {0, hit};
        }
    }

    // returns (to,from)
    template <UTFSize<2> From, UTFSize<1> To>
    constexpr std::pair<size_t, size_t> is_utf16_to_utf8_convertible(To* to, size_t tosize, const From* code, size_t fromsize) {
        const auto hit = is_valid_utf16(code, fromsize);
        switch (hit) {
            case 0:
                return {0, 0};
            case 1: {
                const auto v = expect_utf8_len(code[0]);
                if (!to || tosize < v) {
                    return {0, hit};
                }
                return {v, hit};
            }
            case 2:
                if (!to || tosize < 4) {
                    return {0, hit};
                }
                return {4, 2};
            default:
                return {0, hit};
        }
    }

    // returns (to,from(utf16 count=byte/2))
    template <UTFSize<1> From, UTFSize<1> To>
    constexpr std::pair<size_t, size_t> is_utf16_to_utf8_convertible(To* to, size_t tosize, const From* code, size_t fromsize, bool be) {
        const auto [hit, first] = is_valid_utf16(code, fromsize, be);
        switch (hit) {
            case 0:
                return {0, 0};
            case 1: {
                const auto v = expect_utf8_len(first);
                if (!to || tosize < v) {
                    return {0, hit};
                }
                return {v, hit};
            }
            case 2:
                if (!to || tosize < 4) {
                    return {0, hit};
                }
                return {4, 2};
            default:
                return {0, hit};
        }
    }

    namespace test {
        constexpr bool check_u8u16() {
            byte data1[4]{0};
            std::uint16_t data2[2]{u'や'};
            byte data3[4]{0};
            auto [to, from] = is_utf16_to_utf8_convertible(data1, 4, data2, 2);
            if (to != 3 || from != 1 ||
                !compose_utf8_from_utf16(data1, to, data2, from)) {
                return false;
            }
            std::tie(to, from) = is_utf8_to_utf16_convertible(data3, 4, data1, 4);
            if (to != 1 || from != 3 ||
                !compose_utf16_from_utf8(data3, to, data1, from, true)) {
                return false;
            }
            if (!compose_utf8_from_utf16(data1, from, data3, to, true)) {
                return false;
            }
            if (compose_utf32_from_utf16(data3, to, true) != data2[0]) {
                return false;
            }
            if (compose_utf32_from_utf8(data1, from) != data2[0]) {
                return false;
            }
            return true;
        }

        static_assert(check_u8u16(), "test failed");
    }  // namespace test

}  // namespace utils::utf
