/*license*/

// char - char aliases
#pragma once

namespace utils {
    namespace wrap {
#ifdef _WIN32
        using path_char = wchar_t;
#else
        using path_char = char;
#endif
    }  // namespace wrap
}  // namespace utils