/*license*/

// cout - wrapper of cout
#pragma once

#include <iosfwd>
#include <sstream>
#include "lite/char.h"
#include "lite/string.h"
#include "../helper/sfinae.h"
#include "../core/buffer.h"

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

            DEFINE_SFINAE_T(is_string, std::declval<Buffer<typename BufferType<T&>::type>>());

            void write(const path_char* str);
        };
    }  // namespace wrap
}  // namespace utils
