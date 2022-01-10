/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "../../include/platform/windows/dllexport_source.h"
#include "../../include/wrap/iocommon.h"
#include "../../include/wrap/cin.h"
#include <cstdio>
#include <iostream>
#ifdef _WIN32
#include <conio.h>
#include <io.h>
#include "Windows.h"
#else
#define _O_U8TEXT 0
#endif

namespace utils {
    namespace wrap {
        ::FILE* is_std(istream&);

        UtfIn::UtfIn(istream& i)
            : in(in) {
            std_handle = is_std(i);
        }

        UtfIn& UtfIn::operator>>(path_string& out) {
            std::getline(in, out);
            return *this;
        }

        bool UtfIn::has_input() const {
            if (std_handle) {
#ifdef _WIN32
                if (::_isatty(0)) {
                    if (_kbhit()) {
                        return true;
                    }
                }
#endif
            }
            else {
                in.get();
                return true;
            }
        }

    }  // namespace wrap
}  // namespace utils