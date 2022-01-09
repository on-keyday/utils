/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// literals - json literals
#pragma once
#include "json.h"

namespace utils {
    namespace json {
        namespace literals {
            JSON operator""_json(const char* s, size_t sz) {
                return parse<JSON>(helper::SizedView(s, sz));
            }

            OrderedJSON operator""_ojson(const char* s, size_t sz) {
                return parse<OrderedJSON>(helper::SizedView(s, sz));
            }
        }  // namespace literals
    }      // namespace json
}  // namespace utils
