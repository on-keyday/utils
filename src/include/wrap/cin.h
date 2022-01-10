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
#include "lite/char.h"
#include "lite/string.h"
#include "../thread/lite_lock.h"
#include "pack.h"
#include "lite/stream.h"

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

            bool has_input() const;
        };
    }  // namespace wrap
}  // namespace utils
