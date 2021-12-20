/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// deref - dereference wrapper
#pragma once

#include "sfinae.h"
#include <memory>

namespace utils {
    namespace helper {
        namespace internal {
            SFINAE_BLOCK_T_BEGIN(derefable, *std::declval<T>())
            using type = decltype(std::addressof(*std::declval<T&>()));
            static auto deref(T& t) {
                return std::addressof(*t);
            }
            SFINAE_BLOCK_T_ELSE(derefable)
            using type = decltype(std::addressof(std::declval<T&>()));
            static auto deref(T& t) {
                return std::addressof(t);
            }
            SFINAE_BLOCK_T_END()
        }  // namespace internal
    }      // namespace helper
}  // namespace utils
