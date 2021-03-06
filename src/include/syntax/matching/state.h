/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// state - matching state
#pragma once

namespace utils {
    namespace syntax {
        enum class MatchState {
            succeed,
            not_match,
            fatal,
        };
    }  // namespace syntax
}  // namespace utils
