/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// json - json object
#pragma once

#include "internal.h"

namespace utils {
    namespace net {
        namespace json {
            template <class String, template <class...> class Vec, template <class...> class Object>
            struct JSONBase {
                JSONKind kind = JSONKind::undefined;
                internal::JSONHolder<String, Vec, Object> obj;
            };
        }  // namespace json
    }      // namespace net
}  // namespace utils
