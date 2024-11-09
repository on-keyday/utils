/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// input - easy input function
#include <platform/windows/dllexport_source.h>
#include <wrap/cin.h>
#include <wrap/cout.h>
#include <platform/detect.h>
#ifdef FUTILS_PLATFORM_WINDOWS
#include <Windows.h>
#endif
#include <unicode/utf/convert.h>
#include <helper/defer.h>
#include <file/file.h>

namespace futils {
    namespace wrap {

#ifdef FUTILS_PLATFORM_WINDOWS
        /*
        static bool read_record(wrap::path_string& buf, INPUT_RECORD& rec, bool& zero_input, wrap::InputState& state) {
            auto& cout = wrap::cout_wrap();
            auto echo_back = [&](auto&& buf) {
                if (cout.is_tty()) {
                    cout << buf;
                }
            };
            bool ret = true;

            if (rec.EventType == KEY_EVENT) {
                auto& key = rec.Event.KeyEvent;
                if (!key.bKeyDown && !zero_input) {
                    return true;
                }
                zero_input = false;
                auto ctrl_key = key.dwControlKeyState & RIGHT_CTRL_PRESSED || key.dwControlKeyState & LEFT_CTRL_PRESSED;
                auto C = key.uChar.UnicodeChar;
                for (auto i = 0; i < key.wRepeatCount; i++) {
                    if (C == '\b') {
                        if (buf.size()) {
                            buf.pop_back();
                            state.buffer_update = true;
                        }
                        if (state.edit_buffer && state.edit_buffer->size()) {
                            state.edit_buffer->pop_back();
                        }
                        if (!state.no_echo) {
                            echo_back("\b \b");
                        }
                        continue;
                    }
                    if (C == 0 && !ctrl_key) {
                        zero_input = true;
                        return true;
                    }
                    if (!state.no_echo) {
                        wchar_t last = buf.size() ? buf.back() : 0;
                        if (unicode::utf16::is_high_surrogate(C)) {
                            // skip
                        }
                        else if (unicode::utf16::is_high_surrogate(last) && unicode::utf16::is_low_surrogate(C)) {
                            wchar_t r[]{last, C, 0};
                            echo_back(r);
                        }
                        else {
                            wchar_t r[]{C, 0};
                            if (!ctrl_key || C != 3) {  // skip ctrl-C
                                echo_back(r);
                            }
                        }
                    }
                    if (C == '\r') {
                        if (!state.no_echo) {
                            echo_back("\r");
                        }
                        C = '\n';
                    }
                    buf.push_back(C);
                    if (state.edit_buffer) {
                        state.edit_buffer->push_back(C);
                    }
                    state.buffer_update = true;
                    if (C == '\n') {
                        ret = false;
                    }
                    if (ctrl_key && C == 3) {  // ctrl C
                        if (!state.no_echo) {
                            echo_back("^C\n");
                        }
                        state.ctrl_c = true;
                        ret = false;
                    }
                }
            }
            return ret;
        }

        static bool input_platform(wrap::path_string& buf, wrap::InputState& state) {
            auto handle = GetStdHandle(STD_INPUT_HANDLE);
            DWORD flag = 0;
            GetConsoleMode(handle, &flag);
            auto raii = helper::defer([&] {
                SetConsoleMode(handle, flag);
            });
            flag &= ~ENABLE_PROCESSED_INPUT;
            SetConsoleMode(handle, flag);
            bool zero_input = false;
            state.buffer_update = false;
            while (true) {
                INPUT_RECORD record;
                DWORD red = 0;
                if (!GetNumberOfConsoleInputEvents(handle, &red)) {
                    return false;
                }
                if (red == 0) {
                    if (state.non_block) {
                        return false;
                    }
                    Sleep(2);
                    continue;
                }
                for (auto i = 0; i < red; i++) {
                    DWORD n;
                    if (!ReadConsoleInputW(handle, &record, 1, &n)) {
                        return false;
                    }
                    if (!read_record(buf, record, zero_input, state)) {
                        return true;
                    }
                }
                Sleep(1);
            }
        }
        */

        static void enable_ctrl_c_platform(bool en, unsigned int& flag) {
            auto handle = GetStdHandle(STD_INPUT_HANDLE);
            DWORD flags;
            GetConsoleMode(handle, &flags);
            auto copy = flags;
            flag = flags;
            if (en) {
                copy |= ENABLE_PROCESSED_INPUT;
            }
            else {
                copy &= ~ENABLE_PROCESSED_INPUT;
            }
            SetConsoleMode(handle, copy);
        }
#else
        static bool input_platform(wrap::path_string& buf, wrap::InputState&) {
            return false;
        }
        static void enable_ctrl_c_platform(bool en, unsigned int& flag) {
        }
#endif
        bool STDCALL input(wrap::path_string& buf, InputState* state) {
            auto& fin = file::File::stdin_file();
            if (!fin.is_tty()) {
                return false;
            }
            InputState tmp;
            if (!state) {
                state = &tmp;
            }
            auto b = fin.interactive_console_read();
            if (!b) {
                return false;
            }
            struct Interact {
                wrap::path_string& buf;
                InputState* state;
                bool end = false;
                bool input = false;
                size_t u16_index = 0;
                wrap::path_char tmp[3]{};

                void operator()(std::uint32_t c) {
                    auto& cout = wrap::cout_wrap();
                    auto echo_back = [&](auto&& buf) {
                        if (!state->no_echo && cout.is_tty()) {
                            cout << buf;
                        }
                    };
                    input = true;
                    if (file::is_special_key(c)) {
                        state->special_key = file::ConsoleSpecialKey(c);
                        return;
                    }
                    if (c == '\b') {
                        if (buf.size()) {
                            buf.pop_back();
                            state->buffer_update = true;
                        }
                        if (state->edit_buffer && state->edit_buffer->size()) {
                            state->edit_buffer->pop_back();
                        }
                        echo_back("\b \b");
                        return;
                    }
                    buf.push_back(c);
                    if (state->edit_buffer) {
                        state->edit_buffer->push_back(c);
                    }
                    state->buffer_update = true;
                    if (c == '\n') {
                        echo_back("\r\n");
                        end = true;
                    }
                    else if (c == 3) {
                        echo_back("^C\n");
                        state->ctrl_c = true;
                        end = true;
                    }
                    else {
                        // NOTE: if path_char is not wchar_t,
                        // this code is simply evaluated false and ignored
                        if (futils::unicode::utf16::is_high_surrogate(c)) {
                            if (u16_index == 0) {
                                tmp[0] = c;
                                u16_index++;
                            }
                            else {
                                tmp[1] = c;
                                tmp[2] = 0;
                                echo_back(tmp);
                                u16_index = 0;
                            }
                        }
                        else if (u16_index == 1) {
                            tmp[1] = c;
                            tmp[2] = 0;
                            echo_back(tmp);
                            u16_index = 0;
                        }
                        else {
                            tmp[0] = c;
                            tmp[1] = 0;
                            echo_back(tmp);
                        }
                    }
                }
            } i_act{buf, state};
            for (;;) {
                auto err = b->interact(&i_act, [](std::uint32_t c, void* data) {
                    auto& buf = *reinterpret_cast<Interact*>(data);
                    buf(c);
                });
                if (!err) {
                    return false;
                }
                if (i_act.end) {
                    break;
                }
                if (state->non_block && !i_act.input) {
                    return false;
                }
                i_act.input = false;
            }
            return true;
        }

        void STDCALL enable_ctrl_c(bool en, unsigned int& flag) {
            return enable_ctrl_c_platform(en, flag);
        }

    }  // namespace wrap
}  // namespace futils
