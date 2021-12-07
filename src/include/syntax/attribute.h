/*license*/

// attribute - define attribute
#pragma once

#include "../wrap/lite/enum.h"

namespace utils {
    namespace syntax {
        enum class Attribute {
            repeat = 0x01,
            fatal = 0x02,
            ifexist = 0x04,
            adjacent = 0x08,
        };

        BEGIN_ENUM_STRING_MSG(Attribute, attribute)
        ENUM_ERROR_MSG(Attribute::repeat, "*")
        ENUM_ERROR_MSG(Attribute::fatal, "!")
        ENUM_ERROR_MSG(Attribute::ifexist, "?")
        ENUM_ERROR_MSG(Attribute::adjacent, "&")
        END_ENUM_STRING_MSG("")
    }  // namespace syntax
}  // namespace utils
