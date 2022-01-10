/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "../../include/platform/windows/dllexport_source.h"
#include "../../include/wrap/iocommon.h"
#include "../../include/wrap/cin.h"
#include "../../include/helper/readutil.h"
#include <cstdio>
#include <iostream>
#ifdef _WIN32
#include <conio.h>
#include <io.h>
#include "Windows.h"
#else
#include <sys/select.h>
#include <sys/time.h>
#define _O_U8TEXT 0
#endif

namespace utils {
    namespace wrap {
        ::FILE* is_std(istream&);

        UtfIn::UtfIn(istream& i)
            : in(i) {
            std_handle = is_std(i);
        }

        UtfIn& UtfIn::operator>>(path_string& out) {
            force_init_io();
            std::getline(in, out);
            return *this;
        }

        bool UtfIn::has_input() {
            if (std_handle) {
#ifdef _WIN32
                auto h = ::GetStdHandle(STD_INPUT_HANDLE);
                ::DWORD bytes_left;
                ::PeekNamedPipe(h, NULL, 0, NULL, &bytes_left, NULL);
                return bytes_left != 0;
#else
                ::fd_set set{0};
                ::timeval tv{0};
                FD_ZERO(set, 0)
                FD_SET(set, 0)::select();
                auto e = ::select(1, &set, nullptr, nullptr, &tv);
                return e != 0;
#endif
            }
            else {
                return true;
            }
        }

        UtfIn& STDCALL cin_wrap() {
#ifdef _WIN32
            static UtfIn cin{std::wcin};
#else
            static UtfIn cin{std::cin};
#endif
            return cin;
        }

    }  // namespace wrap
}  // namespace utils