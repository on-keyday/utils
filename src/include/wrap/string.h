/*license*/

// string - wrap default string type
#pragma once

#ifndef UTILS_WRAP_STRING_TYPE
#include <string>
#define UTILS_WRAP_STRING_TYPE std::string
#endif

namespace utils {
    namespace wrap {
        using string = UTILS_WRAP_STRING_TYPE;
    }
}  // namespace utils
