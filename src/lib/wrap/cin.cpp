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
#define _O_U8TEXT 0
#endif

namespace utils {
    namespace wrap {
        ::FILE* is_std(istream&);

        UtfIn::UtfIn(istream& i)
            : in(i) {
            std_handle = is_std(i);
        }

        static path_string glbuf;

        UtfIn& UtfIn::operator>>(path_string& out) {
            force_init_io();
            if (std_handle) {
                while (true) {
                    lock.lock();
                    if (glbuf.size()) {
                        auto seq = make_ref_seq(glbuf);
                        auto e = helper::read_until(out, seq, '\n');
                        glbuf.erase(0, seq.rptr);
                        if (!e) {
                            lock.unlock();
                            continue;
                        }
                    }
                    else {
                        lock.unlock();
                        continue;
                    }
                    break;
                }
                lock.unlock();
            }
            else {
                std::getline(in, out);
            }
            return *this;
        }

        bool UtfIn::has_input() {
            if (std_handle) {
#ifdef _WIN32
                lock.lock();
                if (::_isatty(0)) {
                    while (::_kbhit()) {
                        auto c = ::_getwch();
                        if (c == '\b' && glbuf.size()) {
                            glbuf.pop_back();
                        }
                        else {
                            glbuf.push_back(c);
                        }
                        if (c == '\n') {
                            lock.unlock();
                            return true;
                        }
                    }
                    return false;
                }
                else {
                    return true;
                }
#endif
                return true;
            }
            else {
                auto c = in.get();
                in.unget();
                return c != decltype(c)(EOF);
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