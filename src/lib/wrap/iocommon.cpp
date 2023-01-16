/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
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
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include "Windows.h"
#else
#define _O_U8TEXT 0
#endif

namespace utils {
    namespace wrap {
        static thread::LiteLock gllock;
        static bool initialized = false;

        int stdinmode = _O_U8TEXT;
        int stdoutmode = _O_U8TEXT;
        int stderrmode = _O_U8TEXT;
        bool sync_stdio = false;
        bool out_virtual_terminal = false;
        bool need_cr_for_return = false;
        bool in_virtual_terminal = false;
        bool no_change_mode = false;
        bool handle_input_self = false;

        ::FILE* is_std(istream& in) {
            auto addr = std::addressof(in);
#ifdef _WIN32
            if (addr == std::addressof(std::wcin))
#else
            if (addr == std::addressof(std::cin))
#endif
            {
                return stdin;
            }
            return nullptr;
        }

        ::FILE* is_std(ostream& out) {
            auto addr = std::addressof(out);
#ifdef _WIN32
            if (addr == std::addressof(std::wcout))
#else
            if (addr == std::addressof(std::cout))
#endif
            {
                return stdout;
            }
#ifdef _WIN32
            else if (addr == std::addressof(std::wcerr) ||
                     addr == std::addressof(std::wclog))
#else
            else if (addr == std::addressof(std::cerr) ||
                     addr == std::addressof(std::clog))
#endif
            {
                return stderr;
            }
            return nullptr;
        }

#ifdef _WIN32
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
                auto outhandle = ::GetStdHandle(STD_OUTPUT_HANDLE);
                auto errhandle = ::GetStdHandle(STD_ERROR_HANDLE);
                if (!outhandle || !errhandle ||
                    outhandle == INVALID_HANDLE_VALUE ||
                    errhandle == INVALID_HANDLE_VALUE) {
                    return false;
                }
                auto res = change_output_mode(outhandle) && change_output_mode(errhandle);
                if (!res) {
                    return false;
                }
            }
            if (!out && in_virtual_terminal) {
                auto inhandle = ::GetStdHandle(STD_INPUT_HANDLE);
                if (!inhandle || inhandle == INVALID_HANDLE_VALUE) {
                    return false;
                }
                auto res = change_input_mode(inhandle);
                if (!res) {
                    return false;
                }
            }
            return true;
        }
#endif
        static bool out_init() {
#ifdef _WIN32
            change_console_mode(true);
            if (!no_change_mode) {
                if (_setmode(_fileno(stdout), stdoutmode) == -1) {
                    // err = "error:text output mode change failed\n";
                    return false;
                }
                if (_setmode(_fileno(stderr), stderrmode) == -1) {
                    // err = "error:text error mode change failed\n";
                    return false;
                }
            }
#endif
            return true;
        }

        static bool in_init() {
#ifdef _WIN32
            if (handle_input_self) {
                return true;
            }
            change_console_mode(false);
            if (!no_change_mode) {
                if (_setmode(_fileno(stdin), stdinmode) == -1) {
                    // err = "error:text input mode change failed";
                    return false;
                }
            }
#endif
            return true;
        }

        static bool io_init() {
            if (!in_init() || !out_init()) {
                return false;
            }
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
}  // namespace utils
