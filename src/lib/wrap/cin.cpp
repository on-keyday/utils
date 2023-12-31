/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
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
#include "../../include/wrap/cout.h"
#include <unicode/utf/minibuffer.h>

namespace utils {
    namespace wrap {
        ::FILE* is_std(std::ios_base&);

        static path_string glbuf;
#ifdef UTILS_PLATFORM_WINDOWS
/*
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
          */
#endif

        bool UtfIn::peek_buffer(path_string& buf, bool no_cin, bool* updated) {
            if (is_tty()) {
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
            return true;
        }

        UtfIn& UtfIn::operator>>(path_string& out) {
            if (glbuf.size()) {
                out.append(glbuf);
                glbuf.clear();
                return *this;
            }
            if (is_tty()) {
                out.resize(1024);
                view::basic_wvec<path_char> buf{out};
                size_t len = 0;
                for (;;) {
                    auto f = file.read_console(buf);
                    if (!f) {
                        break;  // error but no way to handle
                    }
                    if (f->size() < out.size()) {
                        len += f->size();
                        out.resize(len);
                        break;
                    }
                    if (f->back() == '\n') {
                        len += f->size();
                        out.resize(len);
                        break;
                    }
                    out.resize(out.size() * 2);
                    buf = view::basic_wvec<path_char>{out.data() + len, out.data() + out.size()};
                    len += f->size();
                }
            }
            else {
                char u8_buf[4]{};
                char input = 0;
                auto read_one = [&] {
                    auto c = file.read_file(view::wvec(&input, 1));
                    if (!c) {
                        return false;
                    }
                    if (c->size() != 1) {
                        return false;
                    }
                    return true;
                };
                for (;;) {
                    read_one();
                    auto len = unicode::utf8::first_byte_to_len(input);
                    if (len == 0) {
                        utf::from_utf32(unicode::replacement_character, out);
                        continue;
                    }
                    if (len == 1) {
                        out.push_back(input);
                        if (input == '\n') {
                            break;
                        }
                        continue;
                    }
                    u8_buf[0] = input;
                    for (auto i = 1; i < len; i++) {
                        if (!read_one()) {
                            break;
                        }
                        u8_buf[i] = input;
                    }
                    utf::MiniBuffer<path_char> c;
                    if (!utf::convert(view::wvec(u8_buf, u8_buf + len), c)) {
                        utf::from_utf32(unicode::replacement_character, out);
                        continue;
                    }
                    out.append(c.c_str(), c.size());
                }
            }
            return *this;
        }

        UtfIn& STDCALL cin_wrap() {
            static UtfIn cin(file::File::stdin_file());
            return cin;
        }

    }  // namespace wrap
}  // namespace utils
