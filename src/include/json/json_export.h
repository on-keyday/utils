/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// json_export - export json
#pragma once

#include "../platform/windows/dllexport_header.h"
#include "json.h"
#include "literals.h"

namespace futils {

    namespace json {
#if !defined(FUTILS_JSON_NO_EXTERN_TEMPLATE)
        extern
#endif
            template struct futils_DLL_EXPORT JSONBase<wrap::string, wrap::vector, wrap::map>;
#if !defined(FUTILS_JSON_NO_EXTERN_TEMPLATE)
        extern
#endif
            template struct futils_DLL_EXPORT JSONBase<wrap::string, wrap::vector, ordered_map>;

#if !defined(FUTILS_JSON_NO_EXTERN_TEMPLATE)
        extern
#endif
            template futils_DLL_EXPORT JSON
            parse<JSON, view::CharVec<const char>>(view::CharVec<const char>&&, bool);

#if !defined(FUTILS_JSON_NO_EXTERN_TEMPLATE)
        extern
#endif
            template futils_DLL_EXPORT OrderedJSON
            parse<OrderedJSON, view::CharVec<const char>>(view::CharVec<const char>&&, bool);
    }  // namespace json

}  // namespace futils
