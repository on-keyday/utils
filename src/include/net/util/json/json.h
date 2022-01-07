/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// json - json object
#pragma once

namespace utils {
    namespace net {
        enum class JSONKind {
            undefined,
            null,
            boolean,
            number_i,
            number_f,
            number_u,
            string,
        };

        struct JSON {
            JSONKind kind = JSONKind::undefined;
            union {
            };
        };
    }  // namespace net
}  // namespace utils
