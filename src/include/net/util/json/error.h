/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// error - define json error
#pragma once
#include "../../../wrap/lite/enum.h"
namespace utils {
    namespace net {
        namespace json {
            enum class JSONError {
                none,
                unknown,
                unexpected_eof,
                invalid_escape,
                need_comma_on_array,
                need_comma_on_object,
                need_key_name,
                need_colon,
                emplace_error,
                invalid_number,
                not_json,
            };

            using JSONErr = wrap::EnumWrap<JSONError, JSONError::none, JSONError::unknown>;

        }  // namespace json

    }  // namespace net
}  // namespace utils