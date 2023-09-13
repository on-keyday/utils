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
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include "Windows.h"
#else
#define _O_U8TEXT 0
#endif

namespace utils {
    namespace wrap {
        ::FILE* is_std(ostream& out);

        UtfOut::UtfOut(ostream& out)
            : out(out) {
            std_handle = is_std(this->out);
        }

        void UtfOut::write(const path_string& p) {
            if (std_handle) {
                force_init_io();
#ifdef _WIN32
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
#ifdef _WIN32
            auto no = ::_fileno(std_handle);
            return ::_isatty(no);
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

        UtfOut& cout_wrap() {
#ifdef _WIN32
            static UtfOut cout(std::wcout);
#else
            static UtfOut cout(std::cout);
#endif
            return cout;
        }

        UtfOut& cerr_wrap() {
#ifdef _WIN32
            static UtfOut cerr(std::wcerr);
#else
            static UtfOut cerr(std::cerr);
#endif
            return cerr;
        }
    }  // namespace wrap
}  // namespace utils
