/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// appender - append to other string
#pragma once

#include "sfinae.h"

namespace utils {
    namespace helper {
        SFINAE_BLOCK_TU_BEGIN(has_append, std::declval<T>().append(std::declval<U>()))
        SFINAE_BLOCK_TU_ELSE(has_append)
        SFINAE_BLOCK_TU_END()
    }  // namespace helper
}  // namespace utils