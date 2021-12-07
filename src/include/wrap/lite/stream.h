/*license*/

// stream - wrap std stream
#pragma once

#include <iosfwd>
namespace utils {
    namespace wrap {
#ifdef _WIN32
        using ostream = std::wostream;
        using stringstream = std::wstringstream;
#else
        using ostream = std::ostream;
        using stringstream = std::stringstream;
#endif
    }  // namespace wrap
}  // namespace utils