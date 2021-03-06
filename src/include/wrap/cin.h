/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
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
        struct DLL UtfIn {
           private:
            istream& in;
            ::FILE* std_handle = nullptr;
            thread::LiteLock lock;

           public:
            UtfIn(istream& i);
            UtfIn& operator>>(path_string& out);

            template <class T>
            UtfIn& operator>>(T& out) {
                path_string str;
                (*this) >> str;
                utf::convert(str, out);
                return *this;
            }

            bool has_input();

            // this works on condition below
            // 1. on windows
            // 2. stdin object
            // 3. stdin is tty
            // no_cin means not buffering to cin
            // if above is not satisfied
            // this function always return true
            bool peek_buffer(path_string& buf, bool no_cin = false, bool* updated = nullptr);
        };

        DLL UtfIn& STDCALL cin_wrap();
    }  // namespace wrap
}  // namespace utils
