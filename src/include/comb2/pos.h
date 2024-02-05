/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstddef>

namespace futils::comb2 {
    struct Pos {
        std::size_t begin = 0;
        std::size_t end = 0;

        constexpr size_t len() const {
            return end - begin;
        }

        constexpr bool npos() const {
            return begin == ~0 && end == ~0;
        }
        };

    constexpr bool operator==(const Pos& lhs, const Pos& rhs) {
        return lhs.begin == rhs.begin && lhs.end == rhs.end;
    }

    constexpr auto npos = Pos{~size_t(0), ~size_t(0)};
}  // namespace futils::comb2
