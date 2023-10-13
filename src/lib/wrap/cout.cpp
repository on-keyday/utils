/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "../../include/platform/windows/dllexport_source.h"
#include "../../include/wrap/iocommon.h"
#include "../../include/wrap/cout.h"
#include <cstdio>
#include <iostream>
#include <platform/detect.h>
#ifdef UTILS_PLATFORM_WINDOWS
#include <fcntl.h>
#include <io.h>
#include "Windows.h"
#else
#define _O_U8TEXT 0
#include <unistd.h>
#endif

namespace utils {
    namespace wrap {
        /*
        ::FILE* is_std(std::ios_base& out);

        UtfOut::UtfOut(ostream& out)
            : out(out) {
            std_handle = is_std(this->out);
        }

        void UtfOut::write(const path_string& p) {
            if (std_handle) {
                force_init_io();
#ifdef UTILS_PLATFORM_WINDOWS
                auto no = ::_fileno(std_handle);
                if (::_isatty(no)) {
                    auto h = ::GetStdHandle(no == 1 ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE);
                    DWORD written;
                    auto e = ::WriteConsoleW(h, p.c_str(), p.size(), &written, nullptr);
                    // for debug code
                    auto err = ::GetLastError();
                    if (err) {
                        (void)err;
                    }
                }
                else
#endif
                {
                    ::fwrite(p.c_str(), sizeof(path_char), p.size(), std_handle);
                    ::fflush(std_handle);
                }
            }
            else {
                this->out << p;
            }
        }

        bool UtfOut::is_tty() const {
#ifdef UTILS_PLATFORM_WINDOWS
            return ::_isatty(::_fileno(std_handle));
#else
            return isatty(fileno(std_handle));
#endif
        }

        UtfOut& UtfOut::operator<<(const path_string& p) {
            write(p);
            return *this;
        }

        UtfOut& UtfOut::operator<<(internal::Pack&& pack) {
            write(pack.impl.result);
            return *this;
        }
        */

        UtfOut& cout_wrap() {
            static UtfOut cout(file::File::stdout_file());
            return cout;
        }

        UtfOut& cerr_wrap() {
            static UtfOut cerr(file::File::stderr_file());
            return cerr;
        }
    }  // namespace wrap
}  // namespace utils
