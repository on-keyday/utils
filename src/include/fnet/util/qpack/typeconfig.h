/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <vector>
#include <deque>
#include <unordered_map>
#include <string>

namespace futils {
    namespace qpack {
        template <class String = std::string,
                  template <class...> class RefMap = std::unordered_map,
                  template <class...> class Vec = std::vector,
                  template <class...> class FieldQue = std::deque>
        struct TypeConfig {
            template <class K, class V>
            using ref_map = RefMap<K, V>;
            template <class V>
            using vec = Vec<V>;
            template <class V>
            using field_que = FieldQue<V>;
            using string = String;
        };
    }  // namespace qpack
}  // namespace futils
