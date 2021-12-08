/*license*/

// cout - wrapper of cout
// need to link libutils
#pragma once

#include <iosfwd>
#include <sstream>
#include "lite/char.h"
#include "lite/string.h"
#include "../thread/lite_lock.h"
#include "pack.h"
#include "lite/stream.h"

namespace utils {
    namespace wrap {

        struct UtfOut {
           private:
            ostream& out;
            stringstream ss;
            ::FILE* std_handle = nullptr;
            thread::LiteLock lock;

           public:
            UtfOut(ostream& out);

            template <class T>
            UtfOut& operator<<(T&& t) {
                return WriteWrapper::write(*this, std::forward<T>(t), ss, &lock);
            }

            UtfOut& operator<<(const path_string&);

            void write(const path_string&);

            UtfOut& operator<<(internal::Pack&& pack);
        };
        extern int stdinmode;
        extern int stdoutmode;
        extern int stderrmode;
        extern bool sync_stdio;
        extern bool out_virtual_terminal;
        extern bool in_virtual_terminal;

        UtfOut& cout_wrap();
        UtfOut& cerr_wrap();
    }  // namespace wrap
}  // namespace utils
