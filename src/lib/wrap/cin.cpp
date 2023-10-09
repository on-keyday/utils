/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "../../include/platform/windows/dllexport_source.h"
#include "../../include/wrap/iocommon.h"
#include "../../include/wrap/cin.h"
#include "../../include/strutil/readutil.h"
#include "../../include/unicode/utf/convert.h"
#include <cstdio>
#include <iostream>
#include <platform/detect.h>
#ifdef UTILS_PLATFORM_WINDOWS
#include <conio.h>
#include <io.h>
#include "Windows.h"
#include "../../include/wrap/cout.h"
#else
#include <sys/select.h>
#include <sys/time.h>
#define _O_U8TEXT 0
#include <unistd.h>
#endif

namespace utils {
    namespace wrap {
        ::FILE* is_std(std::ios_base&);

        UtfIn::UtfIn(istream& i)
            : in(i) {
            std_handle = is_std(i);
        }
        static path_string glbuf;

#ifdef UTILS_PLATFORM_WINDOWS
        void echo_back(auto&& buf) {
            if (cout_wrap().is_tty()) {
                cout_wrap() << buf;
            }
        }

        bool load_to_buf(path_string* prvbuf, thread::LiteLock* lock, bool* updated = nullptr) {
            force_init_io();
            auto h = ::GetStdHandle(STD_INPUT_HANDLE);
            ::INPUT_RECORD rec;
            ::DWORD num = 0, res = 0;
            ::GetNumberOfConsoleInputEvents(h, &num);
            path_string buf;
            bool tr = false;
            if (updated) {
                *updated = false;
            }
            bool zeroed = false;
            wchar_t surrogatebuf[2] = {0};
            for (auto i = 0; i < num; i++) {
                ::ReadConsoleInputW(h, &rec, 1, &res);
                if (rec.EventType == KEY_EVENT &&
                    (rec.Event.KeyEvent.bKeyDown || zeroed)) {
                    zeroed = false;
                    for (auto k = rec.Event.KeyEvent.wRepeatCount; k != 0; k--) {
                        auto c = rec.Event.KeyEvent.uChar.UnicodeChar;
                        if (c == '\b') {
                            bool poped = false;
                            if (buf.size()) {
                                buf.pop_back();
                                poped = true;
                            }
                            else {
                                if (prvbuf) {
                                    if (prvbuf->size()) {
                                        prvbuf->pop_back();
                                        poped = true;
                                    }
                                }
                                if (lock) {
                                    lock->lock();
                                    if (glbuf.size()) {
                                        glbuf.pop_back();
                                        poped = true;
                                    }
                                    lock->unlock();
                                }
                            }
                            if (poped) {
                                echo_back(L"\b \b");
                            }
                        }
                        else {
                            if (unicode::utf16::is_high_surrogate(c) && num - i > 0) {
                                surrogatebuf[0] = c;
                            }
                            else if (unicode::utf16::is_low_surrogate(c)) {
                                surrogatebuf[1] = c;
                                echo_back(surrogatebuf);
                                surrogatebuf[0] = 0;
                                surrogatebuf[1] = 0;
                            }
                            else {
                                if (c == 0) {
                                    auto& event = rec.Event.KeyEvent;
                                    if (!(event.dwControlKeyState & LEFT_CTRL_PRESSED) && !(event.dwControlKeyState & RIGHT_CTRL_PRESSED)) {
                                        zeroed = true;
                                        break;
                                    }
                                }
                                wrap::cout_wrap() << c;
                                if (c == '\r' || c == '\n') {
                                    tr = true;
                                    if (c == '\r') {
                                        wrap::cout_wrap() << '\n';
                                    }
                                    c = '\n';
                                }
                            }
                            buf.push_back(c);
                        }
                        if (updated) {
                            *updated = true;
                        }
                    }
                }
            }
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

        bool UtfIn::peek_buffer(path_string& buf, bool no_cin, bool* updated) {
#ifdef UTILS_PLATFORM_WINDOWS
            if (std_handle && ::_isatty(0)) {
                InputState state;
                state.non_block = true;
                if (!no_cin) {
                    state.edit_buffer = &glbuf;  // TODO(on-keyday): thread unsafe
                }
                bool res = input(buf, &state);
                if (updated) {
                    *updated = state.buffer_update;
                }
                return res;
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
                    auto e = strutil::read_until(out, seq, "\n");
                    if (!e) {
                        lock.unlock();
                        std::getline(in, out);
                        break;
                    }
                    out.push_back('\n');
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
#ifdef UTILS_PLATFORM_WINDOWS
                if (::_isatty(0)) {
                    DWORD num = 0;
                    return ::GetNumberOfConsoleInputEvents(::GetStdHandle(STD_INPUT_HANDLE), &num) != 0 &&
                           num != 0;
                }
#else
                ::fd_set set{0};
                ::timeval tv{0};
                FD_ZERO(&set);
                FD_SET(0, &set);
                auto e = ::select(1, &set, nullptr, nullptr, &tv);
                return e != 0;
#endif
            }
            return true;
        }

        UtfIn& STDCALL cin_wrap() {
#ifdef UTILS_PLATFORM_WINDOWS
            static UtfIn cin{std::wcin};
#else
            static UtfIn cin{std::cin};
#endif
            return cin;
        }

        bool UtfIn::is_tty() const {
#ifdef UTILS_PLATFORM_WINDOWS
            return ::_isatty(::_fileno(std_handle));
#else
            return isatty(fileno(std_handle));
#endif
        }

    }  // namespace wrap
}  // namespace utils
