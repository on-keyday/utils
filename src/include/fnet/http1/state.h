/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <core/byte.h>

namespace futils::fnet::http1 {
    enum class HTTPState : byte {
        init,
        first_line,
        header,
        body,
        trailer,
        end,
    };
}