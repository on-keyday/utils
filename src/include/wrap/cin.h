/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// cin - wrap cin
#pragma once
#include "../platform/windows/dllexport_header.h"
#include <iosfwd>
#include "light/char.h"
#include "light/string.h"
#include "../thread/lite_lock.h"
#include "pack.h"
#include "light/stream.h"

namespace utils {
    namespace wrap {
        struct utils_DLL_EXPORT UtfIn {
           private:
            istream& in;
            ::FILE* std_handle = nullptr;
            thread::LiteLock lock;

           public:
            UtfIn(istream& i);
            UtfIn& operator>>(path_string& out);
            size_t read(path_string& out, bool line = true);

            template <class T>
            UtfIn& operator>>(T& out) {
                path_string str;
                (*this) >> str;
                utf::convert(str, out);
                return *this;
            }

            bool has_input();

            bool is_tty() const;

            // this works on condition below
            // 1. on windows
            // 2. stdin object
            // 3. stdin is tty
            // no_cin means not buffering to cin
            // if above is not satisfied
            // this function always return true
            bool peek_buffer(path_string& buf, bool no_cin = false, bool* updated = nullptr);
        };

        utils_DLL_EXPORT UtfIn& STDCALL cin_wrap();

        struct InputState {
            // input
            bool no_echo = false;
            bool non_block = false;

            // output
            bool ctrl_c = false;
            bool buffer_update = false;

            // edit line
            wrap::path_string* edit_buffer = nullptr;
        };

        /// @brief input from stdin with utf8 if console
        /// @param buf  output buffer
        /// @param state input state
        /// @return true if input is terminated with end of line or ctrl-c
        utils_DLL_EXPORT bool STDCALL input(wrap::path_string& buf, InputState* state = nullptr);
        inline bool input(wrap::path_string& buf, bool non_block, bool no_echo = false) {
            InputState state;
            state.non_block = non_block;
            state.no_echo = no_echo;
            return input(buf, &state);
        }
        utils_DLL_EXPORT void STDCALL enable_ctrl_c(bool en, unsigned int& flag);
    }  // namespace wrap
}  // namespace utils
