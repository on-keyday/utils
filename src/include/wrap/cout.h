/*license*/

// cout - wrapper of cout
#pragma once

#include <ios>
#include <sstream>

namespace utils {
    namespace wrap {
#ifdef _WIN32
        using ostream = std::wostream;
#else
        using ostream = std::ostream;
#endif

        struct UtfOut {
            ostream& out;

            void write(const char* str);
        };
    }  // namespace wrap
}  // namespace utils
