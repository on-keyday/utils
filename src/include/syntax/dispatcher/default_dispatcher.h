/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// default_dispatcher - default dispatcher
#pragma once

#include "../../wrap/lite/string.h"
#include "../../wrap/lite/vector.h"
#include "dispatch_context.h"

namespace utils {
    namespace syntax {
        using DefaultDispatcher = DispatchContext<wrap::string, wrap::vector>;
#if !defined(UTILS_SYNTAX_NO_EXTERN_SYNTAXC)
        extern template struct DispatchContext<wrap::string, wrap::vector>;
#endif
    }  // namespace syntax
}  // namespace utils