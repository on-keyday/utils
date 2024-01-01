/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "../../include/platform/windows/dllexport_source.h"
#include "../../include/wrap/iocommon.h"
#include "../../include/thread/lite_lock.h"
#include "../../include/wrap/light/stream.h"
#include <cstdio>
#include <iostream>
#include <cassert>
#include <platform/detect.h>
#ifdef FUTILS_PLATFORM_WINDOWS
#include <fcntl.h>
#include <io.h>
#include "Windows.h"
#else
#define _O_U8TEXT 0
#endif

namespace futils {
    namespace wrap {
        static thread::LiteLock gllock;
        static bool initialized = false;

        int stdin_mode = _O_U8TEXT;
        int stdout_mode = _O_U8TEXT;
        int stderr_mode = _O_U8TEXT;
        bool sync_stdio = false;
        bool out_virtual_terminal = false;
        bool need_cr_for_return = false;
        bool in_virtual_terminal = false;
        bool no_change_mode = false;
        bool handle_input_self = false;

        ::FILE* is_std(std::ios_base& io) {
            auto addr = std::addressof(io);
#ifdef FUTILS_PLATFORM_WINDOWS
            if (addr == std::addressof(std::wcin)) {
                return stdin;
            }
            else if (addr == std::addressof(std::wcout)) {
                return stdout;
            }
            else if (addr == std::addressof(std::wcerr) ||
                     addr == std::addressof(std::wclog)) {
                return stderr;
            }
#else
            if (addr == std::addressof(std::cin)) {
                return stdin;
            }
            else if (addr == std::addressof(std::cout)) {
                return stdout;
            }
            else if (addr == std::addressof(std::cerr) ||
                     addr == std::addressof(std::clog)) {
                return stderr;
            }
#endif
            return nullptr;
        }

#ifdef FUTILS_PLATFORM_WINDOWS
        static bool change_output_mode(auto handle) {
            ::DWORD original;
            if (!GetConsoleMode(handle, &original)) {
                return false;
            }
            ::DWORD require = ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            if (need_cr_for_return) {
                require |= DISABLE_NEWLINE_AUTO_RETURN;
            }
            if (!SetConsoleMode(handle, original | require)) {
                require = ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                if (!SetConsoleMode(handle, original | require)) {
                    return false;
                }
            }
            return true;
        }

        static bool change_input_mode(auto handle) {
            ::DWORD original;
            if (!GetConsoleMode(handle, &original)) {
                return false;
            }
            ::DWORD require = ENABLE_VIRTUAL_TERMINAL_INPUT;
            if (!SetConsoleMode(handle, original | require)) {
                return false;
            }
            return true;
        }

        static bool change_console_mode(bool out) {
            if (out && out_virtual_terminal) {
                auto out_handle = ::GetStdHandle(STD_OUTPUT_HANDLE);
                auto err_handle = ::GetStdHandle(STD_ERROR_HANDLE);
                if (!out_handle || !err_handle ||
                    out_handle == INVALID_HANDLE_VALUE ||
                    err_handle == INVALID_HANDLE_VALUE) {
                    return false;
                }
                auto res = change_output_mode(out_handle) && change_output_mode(err_handle);
                if (!res) {
                    return false;
                }
            }
            if (!out && in_virtual_terminal) {
                auto in_handle = ::GetStdHandle(STD_INPUT_HANDLE);
                if (!in_handle || in_handle == INVALID_HANDLE_VALUE) {
                    return false;
                }
                auto res = change_input_mode(in_handle);
                if (!res) {
                    return false;
                }
            }
            return true;
        }

        static bool out_init() {
            change_console_mode(true);
            if (!no_change_mode) {
                if (_setmode(_fileno(stdout), stdout_mode) == -1) {
                    // err = "error:text output mode change failed\n";
                    return false;
                }
                if (_setmode(_fileno(stderr), stderr_mode) == -1) {
                    // err = "error:text error mode change failed\n";
                    return false;
                }
            }

            return true;
        }

        static bool in_init() {
            if (handle_input_self) {
                return true;
            }
            change_console_mode(false);
            if (!no_change_mode) {
                if (_setmode(_fileno(stdin), stdin_mode) == -1) {
                    // err = "error:text input mode change failed";
                    return false;
                }
            }

            return true;
        }
#endif
        static bool io_init() {
#ifdef FUTILS_PLATFORM_WINDOWS
            if (!in_init() || !out_init()) {
                return false;
            }
#endif
            std::ios_base::sync_with_stdio(sync_stdio);
            initialized = true;
            return true;
        }

        void force_init_io() {
            if (!initialized) {
                gllock.lock();
                if (!initialized) {
                    auto result = io_init();
                    assert(result && "io init failed");
                }
                gllock.unlock();
            }
        }

    }  // namespace wrap
}  // namespace futils
