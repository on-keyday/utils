/*license*/

// cout - wrapper of cout
#pragma once

#include <iosfwd>
#include <sstream>
#include "lite/char.h"
#include "lite/string.h"
#include "../helper/sfinae.h"
#include "../core/buffer.h"
#include "../utf/convert.h"

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

            template <class T, bool flag = is_string<T>::value>
            UtfOut& operator<<(T&& t) {
                wrap::path_string tmp;
                utf::convert<true>(t, tmp);
                write(tmp.c_str());
                return *this;
            }

            template <class T>
            UtfOut& operator<<(T&& t) {
            }

            void write(const path_char* str);
        };
    }  // namespace wrap
}  // namespace utils
