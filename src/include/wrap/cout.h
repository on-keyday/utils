/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// cout - wrapper of cout
// need to link libutils
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

        struct DLL UtfOut {
           private:
            ostream& out;
            stringstream ss;
            ::FILE* std_handle = nullptr;
            thread::LiteLock lock;

           public:
            UtfOut(ostream& out);

            template <class T>
            UtfOut& operator<<(T&& t) {
                return WriteWrapper::write(*this, std::forward<T>(t), ss, &lock);
            }

            UtfOut& operator<<(const path_string&);

            void write(const path_string&);

            UtfOut& operator<<(internal::Pack&& pack);
        };

        DLL UtfOut& STDCALL cout_wrap();
        DLL UtfOut& STDCALL cerr_wrap();

    }  // namespace wrap
}  // namespace utils
