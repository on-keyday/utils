/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// input - easy input function
#include <platform/windows/dllexport_source.h>
#include <wrap/cin.h>
#include <wrap/cout.h>
#ifdef _WIN32
#include <Windows.h>
#endif
#include <utf/convert.h>
#include <helper/defer.h>
using namespace utils;
#ifdef _WIN32
static bool read_record(wrap::path_string& buf, INPUT_RECORD& rec, bool& zero_input, bool no_echo) {
    auto& cout = wrap::cout_wrap();
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
                    if (!no_echo) {
                        cout << "\b \b";
                    }
                }
                continue;
            }
            if (C == 0 && !ctrl_key) {
                zero_input = true;
                return true;
            }
            if (!no_echo) {
                wchar_t last = buf.size() ? buf.back() : 0;
                if (utf::is_utf16_high_surrogate(C)) {
                    // skip
                }
                else if (utf::is_utf16_low_surrogate(C) && utf::is_utf16_high_surrogate(C)) {
                    wchar_t r[]{last, C, 0};
                    cout << r;
                }
                else {
                    wchar_t r[]{C, 0};
                    cout << r;
                }
            }
            if (C == '\r') {
                if (!no_echo) {
                    cout << "\n";
                }
                C = '\n';
            }
            buf.push_back(C);
            if (C == '\n') {
                ret = false;
            }
            if (ctrl_key && C == 3) {  // ctrl C
                if (!no_echo) {
                    cout << "^C\n";
                }
                ret = false;
            }
        }
    }
    return ret;
}

static bool input_platform(wrap::path_string& buf, bool non_block, bool no_echo) {
    auto handle = GetStdHandle(STD_INPUT_HANDLE);
    DWORD flag = 0;
    GetConsoleMode(handle, &flag);
    auto raii = helper::defer([&] {
        SetConsoleMode(handle, flag);
    });
    flag &= ~ENABLE_PROCESSED_INPUT;
    SetConsoleMode(handle, flag);
    bool zero_input = false;
    bool read_least_one = false;
    while (true) {
        INPUT_RECORD record;
        DWORD red = 0;
        if (!GetNumberOfConsoleInputEvents(handle, &red)) {
            return false;
        }
        if (red == 0) {
            if (non_block) {
                return read_least_one;
            }
            Sleep(2);
            continue;
        }
        for (auto i = 0; i < red; i++) {
            DWORD n;
            if (!ReadConsoleInputW(handle, &record, 1, &n)) {
                return false;
            }
            read_least_one = true;
            if (!read_record(buf, record, zero_input, no_echo)) {
                return true;
            }
        }
        Sleep(1);
    }
}
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
static bool input_platform(wrap::path_string& buf, bool non_block, bool no_echo) {
    return false;
}
static void enable_ctrl_c_platform(bool en, unsigned int& flag) {
}
#endif
namespace utils {
    namespace wrap {
        bool STDCALL input(wrap::path_string& buf, bool non_block, bool no_echo) {
            return input_platform(buf, non_block, no_echo);
        }
        void STDCALL enable_ctrl_c(bool en, unsigned int& flag) {
            return enable_ctrl_c_platform(en, flag);
        }

    }  // namespace wrap
}  // namespace utils
