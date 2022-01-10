/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "../../include/platform/windows/dllexport_source.h"
#include "../../include/wrap/cout.h"
#include <cstdio>
#include <iostream>
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
        bool in_virtual_terminal = false;
        bool no_change_mode = false;

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
            ::DWORD require = ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;
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

        static bool change_console_mode() {
            auto outhandle = ::GetStdHandle(STD_OUTPUT_HANDLE);
            auto inhandle = ::GetStdHandle(STD_INPUT_HANDLE);
            auto errhandle = ::GetStdHandle(STD_ERROR_HANDLE);
            if (!outhandle || !inhandle || !errhandle ||
                outhandle == INVALID_HANDLE_VALUE || inhandle == INVALID_HANDLE_VALUE ||
                errhandle == INVALID_HANDLE_VALUE) {
                return false;
            }

            if (out_virtual_terminal) {
                auto res = change_output_mode(outhandle) && change_output_mode(errhandle);
                if (!res) {
                    return false;
                }
            }
            if (in_virtual_terminal) {
                auto res = change_input_mode(inhandle);
                if (!res) {
                    return false;
                }
            }
            return true;
        }
#endif

        static bool io_init() {
#ifdef _WIN32
            change_console_mode();
            if (!no_change_mode) {
                if (_setmode(_fileno(stdin), stdinmode) == -1) {
                    //err = "error:text input mode change failed";
                    return false;
                }
                if (_setmode(_fileno(stdout), stdoutmode) == -1) {
                    //err = "error:text output mode change failed\n";
                    return false;
                }
                if (_setmode(_fileno(stderr), stderrmode) == -1) {
                    //err = "error:text error mode change failed\n";
                    return false;
                }
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
}  // namespace utils