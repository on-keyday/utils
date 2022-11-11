/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// condincr - conditional increment of iterator
#pragma once

namespace utils {
    namespace helper {
        constexpr auto cond_incr(bool& cond) {
            return [&](auto& it) {
                if (!cond) {
                    it++;
                }
                cond = false;
            };
        }
    }  // namespace helper
}  // namespace utils
