/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// attribute - define attribute
#pragma once

#include "../../wrap/lite/enum.h"

namespace utils {
    namespace syntax {
        enum class Attribute {
            none = 0,
            repeat = 0x01,
            fatal = 0x02,
            ifexists = 0x04,
            adjacent = 0x08,
        };

        DEFINE_ENUM_FLAGOP(Attribute)

        BEGIN_ENUM_STRING_MSG(Attribute, attribute)
        ENUM_ERROR_MSG(Attribute::repeat, "*")
        ENUM_ERROR_MSG(Attribute::fatal, "!")
        ENUM_ERROR_MSG(Attribute::ifexists, "?")
        ENUM_ERROR_MSG(Attribute::adjacent, "&")
        END_ENUM_STRING_MSG("")
    }  // namespace syntax
}  // namespace utils
