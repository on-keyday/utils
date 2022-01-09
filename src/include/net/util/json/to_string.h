/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// to_string -json to string
#pragma once
#include "error.h"
#include "jsonbase.h"
#include "../../../number/to_string.h"
#include "../../../helper/appender.h"

namespace utils {
    namespace net {
        namespace json {
            template <class Out, class String, template <class...> class Vec, template <class...> class Object>
            JSONErr to_string(const JSONBase<String, Vec, Object>& json, Out& out) {
                internal::JSONHolder<String, Vec, Object>& holder = json.get_holder();
                if (auto b = holder.as_bool()) {
                    helper::append(out, *b ? "true" : "false");
                    return true;
                }
            }
        }  // namespace json
    }      // namespace net
}  // namespace utils
