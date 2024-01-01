/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// admin - getting role of admin
#pragma once
#include <platform/windows/dllexport_header.h>

namespace futils::wrap {
    enum class RunResult {
        started,
        failed,
        already_admin,
    };

    futils_DLL_EXPORT RunResult run_this_as_admin();
}  // namespace futils::wrap
