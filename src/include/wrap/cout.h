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

            SFINAE_BLOCK_T_BEGIN(is_string, std::declval<Buffer<typename BufferType<T&>::type>>())
            static UtfOut& invoke(UtfOut& out, T&& t) {
                wrap::path_string tmp;
                utf::convert<true>(t, tmp);
                out.write(tmp.c_str());
                return out;
            }
            SFINAE_BLOCK_T_ELSE(is_string)
            SFINAE_BLOCK_T_END()
            template <class T, bool flag = is_string<T>::value>
            UtfOut& operator<<(T&& t) {
            }

            template <class T>
            UtfOut& operator<<(T&& t) {
            }

            void write(const path_char* str);
        };
    }  // namespace wrap
}  // namespace utils
