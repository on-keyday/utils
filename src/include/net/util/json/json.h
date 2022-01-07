/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// json - json object
#pragma once
#include <cstdint>

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
            object,
            array,
        };

        template <class String, template <class...> class Vec, template <class...> class Object>
        struct JSONBase {
            JSONKind kind = JSONKind::undefined;
            union {
                std::int64_t i;
                std::uint64_t u;
            };
        };
    }  // namespace net
}  // namespace utils
