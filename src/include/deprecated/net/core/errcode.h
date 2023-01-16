/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// errcode - error code
#pragma once
#include <platform/windows/dllexport_header.h>

namespace utils {
    namespace net {
        DLL int STDCALL errcode();
    }
}  // namespace utils
