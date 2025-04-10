/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <platform/windows/dllexport_source.h>
#include <platform/detect.h>
#include <file/file.h>
#ifdef FUTILS_PLATFORM_WINDOWS
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <fcntl.h>
#endif

namespace futils::file {
    struct Callback {
        void (*callback)(std::uint32_t, void*);
        void* data;
    };

#if defined(FUTILS_PLATFORM_WINDOWS)

    static bool read_record(INPUT_RECORD& rec, bool& zero_input, Callback cb) {
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
                    cb.callback('\b', cb.data);
                    continue;
                }
                if (C == 0 && !ctrl_key) {
                    if (key.wVirtualKeyCode == VK_UP) {
                        cb.callback(key_up, cb.data);
                        continue;
                    }
                    if (key.wVirtualKeyCode == VK_DOWN) {
                        cb.callback(key_down, cb.data);
                        continue;
                    }
                    if (key.wVirtualKeyCode == VK_LEFT) {
                        cb.callback(key_left, cb.data);
                        continue;
                    }
                    if (key.wVirtualKeyCode == VK_RIGHT) {
                        cb.callback(key_right, cb.data);
                        continue;
                    }
                    zero_input = true;
                    return true;
                }
                if (C == '\r') {
                    // at here, if '\r' is input, '\n' is not input
                    C = '\n';
                }
                cb.callback(C, cb.data);
                if (C == '\n' || ctrl_key && C == 3) {
                    ret = false;
                }
            }
        }
        return ret;
    }

    file_result<void> ConsoleBuffer::interact(void* data, void (*callback)(std::uint32_t, void*)) {
        auto h = reinterpret_cast<HANDLE>(handle);
        for (;;) {
            INPUT_RECORD record;
            DWORD red = 0;
            if (!GetNumberOfConsoleInputEvents(h, &red)) {
                return helper::either::unexpected{FileError{.method = "GetNumberOfConsoleInputEvents", .err_code = GetLastError()}};
            }
            if (red == 0) {
                return {};
            }
            for (auto i = 0; i < red; i++) {
                DWORD n;
                if (!ReadConsoleInputW(h, &record, 1, &n)) {
                    return helper::either::unexpected{FileError{.method = "ReadConsoleInputW", .err_code = GetLastError()}};
                }
                if (!read_record(record, zero_input, Callback{callback, data})) {
                    return {};
                }
            }
        }
    }

    ConsoleBuffer::~ConsoleBuffer() {
        SetConsoleMode(reinterpret_cast<HANDLE>(handle), base_flag);
    }

    file_result<ConsoleBuffer> File::interactive_console_read() const {
        DWORD m_mode = 0;
        if (!GetConsoleMode(reinterpret_cast<HANDLE>(handle), &m_mode)) {
            return helper::either::unexpected{FileError{.method = "GetConsoleMode", .err_code = GetLastError()}};
        }
        if (!SetConsoleMode(reinterpret_cast<HANDLE>(handle), m_mode & ~ENABLE_PROCESSED_INPUT)) {
            return helper::either::unexpected{FileError{.method = "SetConsoleMode", .err_code = GetLastError()}};
        }
        return ConsoleBuffer{handle, m_mode};
    }

    file_result<void> File::set_virtual_terminal_output(bool flag) const {
        DWORD m_mode = 0;
        if (!GetConsoleMode(reinterpret_cast<HANDLE>(handle), &m_mode)) {
            return helper::either::unexpected{FileError{.method = "GetConsoleMode", .err_code = GetLastError()}};
        }
        if (flag) {
            m_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        }
        else {
            m_mode &= ~ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        }
        if (!SetConsoleMode(reinterpret_cast<HANDLE>(handle), m_mode)) {
            return helper::either::unexpected{FileError{.method = "SetConsoleMode", .err_code = GetLastError()}};
        }
        return {};
    }

    file_result<void> File::set_virtual_terminal_input(bool flag) const {
        DWORD m_mode = 0;
        if (!GetConsoleMode(reinterpret_cast<HANDLE>(handle), &m_mode)) {
            return helper::either::unexpected{FileError{.method = "GetConsoleMode", .err_code = GetLastError()}};
        }
        if (flag) {
            m_mode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
        }
        else {
            m_mode &= ~ENABLE_VIRTUAL_TERMINAL_INPUT;
        }
        if (!SetConsoleMode(reinterpret_cast<HANDLE>(handle), m_mode)) {
            return helper::either::unexpected{FileError{.method = "SetConsoleMode", .err_code = GetLastError()}};
        }
        return {};
    }

#else

    ConsoleBuffer::~ConsoleBuffer() {
        struct termios old_settings, new_settings;
        tcgetattr(handle, &old_settings);

        // Copy the old settings to the new settings
        new_settings = old_settings;

        // Restore flags
        new_settings.c_lflag = base_flag;
        new_settings.c_cc[VMIN] = rec[0];
        new_settings.c_cc[VTIME] = rec[1];

        // Apply the new settings
        tcsetattr(handle, TCSANOW, &new_settings);

        // Restore the file descriptor
        fcntl(handle, F_SETFL, rec2);
    }

    file_result<void> ConsoleBuffer::interact(void* data, void (*callback)(std::uint32_t, void*)) {
        ::fd_set set;
        FD_ZERO(&set);
        FD_SET(handle, &set);
        timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;
        for (;;) {
            auto ret = select(handle + 1, &set, nullptr, nullptr, &timeout);
            if (ret == -1) {
                return helper::either::unexpected{FileError{.method = "select", .err_code = errno}};
            }
            if (ret == 0) {
                return {};
            }
            char c[3];
            auto red = read(handle, c, 3);
            if (red == -1) {
                return helper::either::unexpected{FileError{.method = "read", .err_code = errno}};
            }
            if (red == 0) {
                return {};
            }
            if (red == 3) {
                if (c[0] == '\033' && c[1] == '[') {
                    switch (c[2]) {
                        case 'A':
                            callback(key_up, data);
                            continue;
                        case 'B':
                            callback(key_down, data);
                            continue;
                        case 'C':
                            callback(key_right, data);
                            continue;
                        case 'D':
                            callback(key_left, data);
                            continue;
                        default:
                            break;  // ignore
                    }
                }
            }
            else if (red == 1 && c[0] == 0x7f) {
                c[0] = '\b';  // backspace
            }
            for (size_t i = 0; i < red; i++) {
                callback(c[i], data);
            }
        }
    }

    file_result<ConsoleBuffer> File::interactive_console_read() const {
        if (!isatty(handle)) {
            return helper::either::unexpected{FileError{.method = "isatty", .err_code = ENOTTY}};
        }
        struct termios old_settings, new_settings;
        tcgetattr(handle, &old_settings);

        // Copy the old settings to the new settings
        new_settings = old_settings;

        // Disable echo back
        new_settings.c_lflag &= ~(ECHO | ICANON | ISIG);
        new_settings.c_cc[VMIN] = 1;
        new_settings.c_cc[VTIME] = 0;

        // Apply the new settings
        tcsetattr(handle, TCSANOW, &new_settings);

        auto savefl = fcntl(handle, F_GETFL, 0);
        fcntl(handle, F_SETFL, savefl | O_NONBLOCK);

        return ConsoleBuffer{handle, old_settings.c_lflag, old_settings.c_cc[VMIN], old_settings.c_cc[VTIME], static_cast<uint32_t>(savefl)};
    }

    file_result<void> File::set_virtual_terminal_output(bool flag) const {
        return {};
    }

    file_result<void> File::set_virtual_terminal_input(bool flag) const {
        return {};
    }
#endif
}  // namespace futils::file
