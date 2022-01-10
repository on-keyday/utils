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
        static path_string glbuf;

#ifdef _WIN32
        bool load_to_buf(path_string* prvbuf, thread::LiteLock* lock) {
            force_init_io();
            auto h = ::GetStdHandle(STD_INPUT_HANDLE);
            ::INPUT_RECORD rec;
            ::DWORD num = 0, res = 0;
            ::GetNumberOfConsoleInputEvents(h, &num);
            path_string buf;
            bool tr = false;
            for (auto i = 0; i < num; i++) {
                ::PeekConsoleInputW(h, &rec, 1, &res);
                if (rec.EventType == KEY_EVENT &&
                    rec.Event.KeyEvent.bKeyDown) {
                    for (auto i = rec.Event.KeyEvent.wRepeatCount; i != 0; i--) {
                        auto c = rec.Event.KeyEvent.uChar.UnicodeChar;
                        ::fwrite(&c, 2, 1, stdout);
                        if (c == '\b') {
                            ::fwrite(L" \b", 2, 2, stdout);
                            if (buf.size()) {
                                buf.pop_back();
                            }
                            else {
                                if (prvbuf) {
                                    if (prvbuf->size()) {
                                        prvbuf->pop_back();
                                    }
                                }
                                if (lock) {
                                    lock->lock();
                                    if (glbuf.size()) {
                                        glbuf.pop_back();
                                    }
                                    lock->unlock();
                                }
                            }
                        }
                        else {
                            if (c == '\r' || c == '\n') {
                                tr = true;
                                if (c == '\r') {
                                    ::fwrite(L"\n", 2, 1, stdout);
                                }
                                c = '\n';
                            }
                            buf.push_back(c);
                        }
                    }
                }
                ::ReadConsoleInputW(h, &rec, 1, &res);
            }
            ::fflush(stdout);
            if (buf.size()) {
                if (prvbuf) {
                    prvbuf->append(buf);
                }
                if (lock) {
                    lock->lock();
                    glbuf.append(buf);
                    lock->unlock();
                }
            }
            return tr;
        }
#endif

        bool UtfIn::peek_buffer(path_string& buf, bool no_cin) {
#ifdef _WIN32
            if (std_handle && ::_isatty(0)) {
                return load_to_buf(&buf, no_cin ? nullptr : &lock);
            }
#endif
            return true;
        }

        UtfIn& UtfIn::operator>>(path_string& out) {
            force_init_io();
            if (std_handle) {
                while (true) {
                    lock.lock();
                    auto seq = make_ref_seq(glbuf);
                    auto e = helper::read_until(out, seq, "\n");
                    if (!e) {
                        lock.unlock();
                        std::getline(in, out);
                        break;
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
                if (::_isatty(0)) {
                    return load_to_buf(nullptr, &lock);
                }
#else
                ::fd_set set{0};
                ::timeval tv{0};
                FD_ZERO(set, 0)
                FD_SET(set, 0)::select();
                auto e = ::select(1, &set, nullptr, nullptr, &tv);
                return e != 0;
#endif
            }
            return true;
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
