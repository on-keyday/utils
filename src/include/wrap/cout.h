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
#include "../thread/lite_lock.h"

namespace utils {
    namespace wrap {
#ifdef _WIN32
        using ostream = std::wostream;
        using stringstream = std::wstringstream;
#else
        using ostream = std::ostream;
        using stringstream = std::stringstream;
#endif

        struct UtfOut {
           private:
            ostream& out;
            stringstream ss;
            ::FILE* std_handle = nullptr;
            thread::LiteLock lock;

           public:
            UtfOut(ostream& out);

            SFINAE_BLOCK_T_BEGIN(is_string, std::declval<T>()[0])
            static UtfOut& invoke(UtfOut& out, T&& t) {
                path_string tmp;
                utf::convert<true>(t, tmp);
                out.write(tmp);
                return out;
            }
            SFINAE_BLOCK_T_ELSE(is_string)
            static UtfOut& invoke(UtfOut& out, T&& t) {
                out.lock.lock();
                out.ss.str(path_string());
                out.ss << t;
                auto tmp = out.ss.str();
                out.lock.unlock();
                out.write(tmp);
                return out;
            }
            SFINAE_BLOCK_T_END()

            template <class T>
            UtfOut& operator<<(T&& t) {
                return is_string<T>::invoke(*this, std::forward<T>(t));
            }
            void lock();
            void write(const path_string&);
        };
        extern int stdinmode;
        extern int stdoutmode;
        extern int stderrmode;
        extern bool sync_stdio;

        UtfOut& cout_wrap();
        UtfOut& cerr_wrap();
    }  // namespace wrap
}  // namespace utils
