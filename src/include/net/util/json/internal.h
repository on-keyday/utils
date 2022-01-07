/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// internal - json internal definition
#pragma once
#include <cstdint>

namespace utils {
    namespace net {
        namespace json {
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
            struct JSONBase;

            namespace internal {
                template <class String, template <class...> class Vec, template <class...> class Object>
                union JSONHolder {
                    using self_t = JSONBase<String, Vec, Object>;
                    std::int64_t i;
                    std::uint64_t u;
                    double f;
                    String* s;
                    Object<String, self_t>* o;
                    Vec<self_t>* a;
                };
            }  // namespace internal

        }  // namespace json
    }      // namespace net
}  // namespace utils