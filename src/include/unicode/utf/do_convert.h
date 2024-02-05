/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "convert_one.h"

namespace futils::unicode::utf {

    constexpr std::pair<size_t, UTFError> do_convert_one(
        auto&& input, size_t offset, size_t size, auto&& output, bool replace, size_t from, size_t to) {
        switch (from) {
            case 1:
                switch (to) {
                    case 1:
                        return utf8_to_utf8_one(input, offset, size, output, replace);
                    case 2:
                        return utf8_to_utf16_one(input, offset, size, output, replace);
                    case 4:
                        return utf8_to_utf32_one(input, offset, size, output, replace);
                    default:
                        break;
                }
                break;
            case 2:
                switch (to) {
                    case 1:
                        return utf16_to_utf8_one(input, offset, size, output, replace);
                    case 2:
                        return utf16_to_utf16_one(input, offset, size, output, replace);
                    case 4:
                        return utf16_to_utf32_one(input, offset, size, output, replace);
                    default:
                        break;
                }
                break;
            case 4:
                switch (to) {
                    case 1:
                        return utf32_to_utf8_one(input, offset, size, output, replace);
                    case 2:
                        return utf32_to_utf16_one(input, offset, size, output, replace);
                    case 4:
                        return utf32_to_utf32_one(input, offset, size, output, replace);
                    default:
                        break;
                }
                break;
            default:
                break;
        }
        return {0, UTFError::utf_unknown};
    }

    template <size_t from, size_t to>
    constexpr std::pair<size_t, UTFError> do_convert_one(auto&& input, size_t offset, size_t size, auto&& output, bool replace) {
        if constexpr (from == 1) {
            if constexpr (to == 1) {
                return utf8_to_utf8_one(input, offset, size, output, replace);
            }
            else if constexpr (to == 2) {
                return utf8_to_utf16_one(input, offset, size, output, replace);
            }
            else if constexpr (to == 4) {
                return utf8_to_utf32_one(input, offset, size, output, replace);
            }
            else {
                static_assert(to == 1 || to == 2 || to == 4);
            }
        }
        else if constexpr (from == 2) {
            if constexpr (to == 1) {
                return utf16_to_utf8_one(input, offset, size, output, replace);
            }
            else if constexpr (to == 2) {
                return utf16_to_utf16_one(input, offset, size, output, replace);
            }
            else if constexpr (to == 4) {
                return utf16_to_utf32_one(input, offset, size, output, replace);
            }
            else {
                static_assert(to == 1 || to == 2 || to == 4);
            }
        }
        else if constexpr (from == 4) {
            if constexpr (to == 1) {
                return utf32_to_utf8_one(input, offset, size, output, replace);
            }
            else if constexpr (to == 2) {
                return utf32_to_utf16_one(input, offset, size, output, replace);
            }
            else if constexpr (to == 4) {
                return utf32_to_utf32_one(input, offset, size, output, replace);
            }
            else {
                static_assert(to == 1 || to == 2 || to == 4);
            }
        }
        else {
            static_assert(from == 1 || from == 2 || from == 4);
        }
    }

    constexpr std::tuple<size_t, size_t, UTFError> do_convert(
        auto&& input, size_t offset, size_t size, auto&& output, bool replace, size_t from, size_t to) {
        auto do_convert = [&](auto&& cb) -> std::tuple<size_t, size_t, UTFError> {
            size_t i = 0;
            while (i < size) {
                auto [count, ok] = cb(input, offset + i, size - i, output, replace);
                if (ok != UTFError::none) {
                    return {i, count, ok};
                }
                i += count;
            }
            return {i, 0, UTFError::none};
        };
#define DO(from, to) do_convert([&](auto&&... arg) { return do_convert_one<from, to>(arg...); })
        switch (from) {
            case 1:
                switch (to) {
                    case 1:
                        return DO(1, 1);
                    case 2:
                        return DO(1, 2);
                    case 4:
                        return DO(1, 4);
                    default:
                        break;
                }
                break;
            case 2:
                switch (to) {
                    case 1:
                        return DO(2, 1);
                    case 2:
                        return DO(2, 2);
                    case 4:
                        return DO(2, 4);
                    default:
                        break;
                }
                break;
            case 4:
                switch (to) {
                    case 1:
                        return DO(4, 1);
                    case 2:
                        return DO(4, 2);
                    case 4:
                        return DO(4, 4);
                    default:
                        break;
                }
                break;
            default:
                break;
        }
#undef DO
        return {0, 0, UTFError::utf_unknown};
    }

    template <size_t from, size_t to>
    constexpr std::tuple<size_t, size_t, UTFError> do_convert(auto&& input, size_t offset, size_t size, auto&& output, bool replace) {
        size_t i = 0;
        while (i < size) {
            auto [count, ok] = do_convert_one<from, to>(input, offset + i, size - i, output, replace);
            if (ok != UTFError::none) {
                return {i, count, ok};
            }
            i += count;
        }
        return {i, 0, UTFError::none};
    }

}  // namespace futils::unicode::utf
