/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// pool - net woker pool
#pragma once

#include "../../platform/windows/dllexport_header.h"
#include "../../async/worker.h"

namespace utils {
    namespace net {
        DLL async::TaskPool& STDCALL get_pool();
    }
}  // namespace utils
