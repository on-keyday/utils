/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// default_dispatcher - default dispatcher
#pragma once
#include <platform/windows/dllexport_header.h>
#include <wrap/light/lite.h>
#include "dispatch_context.h"

namespace utils {
    namespace syntax {
        using DefaultDispatcher = DispatchContext<wrap::string, wrap::vector>;
#if !defined(UTILS_SYNTAX_NO_EXTERN_SYNTAXC)
        extern
#endif
            template struct DLL DispatchContext<wrap::string, wrap::vector>;
    }  // namespace syntax
}  // namespace utils
