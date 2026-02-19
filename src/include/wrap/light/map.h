/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// map - wrap default map
#pragma once

#if !defined(FUTILS_WRAP_MAP_TEMPLATE) || !defined(FUTILS_WRAP_MULTIMAP_TEMPLATE)
#include <map>
#define FUTILS_WRAP_MAP_TEMPLATE std::map
#define FUTILS_WRAP_MULTIMAP_TEMPLATE std::multimap
#endif

namespace futils {
    namespace wrap {
        template <class Key, class Value>
        using map = FUTILS_WRAP_MAP_TEMPLATE<Key, Value>;
        template <class Key, class Value>
        using multimap = FUTILS_WRAP_MULTIMAP_TEMPLATE<Key, Value>;
    }  // namespace wrap
}  // namespace futils
