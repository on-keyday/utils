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
#include <sstream>
#include "lite/char.h"
#include "lite/string.h"
#include "../thread/lite_lock.h"
#include "pack.h"
#include "lite/stream.h"

namespace utils {
    namespace wrap {
        struct UtfIn {
           private:
            istream& in;
            stringstream ss;
            ::FILE* std_handle = nullptr;
            thread::LiteLock lock;

           public:
            UtfIn(istream& i);
            UtfIn& operator>>(path_string& out);

            bool has_input() const;
        };
    }  // namespace wrap
}  // namespace utils
