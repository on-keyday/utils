/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// literals - json literals
#pragma once
#include "json.h"
#include "../view/charvec.h"

namespace utils {
    namespace json {
        namespace literals {
            inline JSON operator""_json(const char* s, size_t sz) {
                return parse<JSON>(view::CharVec(s, sz));
            }

            inline OrderedJSON operator""_ojson(const char* s, size_t sz) {
                return parse<OrderedJSON>(view::CharVec(s, sz));
            }

        }  // namespace literals
    }      // namespace json
}  // namespace utils
