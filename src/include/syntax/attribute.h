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
    }
}  // namespace utils
