/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// json_export - export json
#pragma once

#include "../platform/windows/dllexport_header.h"
#include "json.h"
#include "literals.h"

namespace utils {

    namespace json {
#if !defined(UTILS_JSON_NO_EXTERN_TEMPLATE)
        extern
#endif
            template struct DLL JSONBase<wrap::string, wrap::vector, wrap::map>;
#if !defined(UTILS_JSON_NO_EXTERN_TEMPLATE)
        extern
#endif
            template struct DLL JSONBase<wrap::string, wrap::vector, ordered_map>;

#if !defined(UTILS_JSON_NO_EXTERN_TEMPLATE)
        extern
#endif
            template DLL JSON
            parse<JSON, view::CharVec<const char>>(view::CharVec<const char>&&, bool);

#if !defined(UTILS_JSON_NO_EXTERN_TEMPLATE)
        extern
#endif
            template DLL OrderedJSON
            parse<OrderedJSON, view::CharVec<const char>>(view::CharVec<const char>&&, bool);
    }  // namespace json

}  // namespace utils
