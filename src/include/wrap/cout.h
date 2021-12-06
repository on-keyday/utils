/*license*/

// cout - wrapper of cout
#pragma once

#include <iosfwd>
#include <sstream>
#include "lite/char.h"
#include "lite/string.h"
#include "../helper/sfinae.h"

namespace utils {
    namespace wrap {
#ifdef _WIN32
        using ostream = std::wostream;
#else
        using ostream = std::ostream;
#endif

        struct UtfOut {
            ostream& out;
            std::stringstream ss;

            void write(const path_char* str);
        };
    }  // namespace wrap
}  // namespace utils
