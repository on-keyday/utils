/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// disable_self - disable self type for template argument constuctor
#pragma once
#include <type_traits>

namespace futils {
    namespace helper {
#define helper_disable_self(Self, Input) std::enable_if_t<!std::is_same_v<Self, std::decay_t<Input>>, int> = 0
    }  // namespace helper
}  // namespace futils
