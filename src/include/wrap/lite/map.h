/*license*/

// map - wrap default map
#pragma once

#if !defined(UTILS_WRAP_MAP_TEMPLATE) || !defined(UTILS_WRAP_MULTIMAP_TEMPLATE)
#include <map>
#define UTILS_WRAP_MAP_TEMPLATE std::map
#define UTILS_WRAP_MULTIMAP_TEMPLATE std::multimap
#endif

namespace utils {
    namespace wrap {
        template <class Key, class Value>
        using map = UTILS_WRAP_MAP_TEMPLATE<Key, Value>;
        template <class Key, class Value>
        using multimap = UTILS_WRAP_MULTIMAP_TEMPLATE<Key, Value>;
    }  // namespace wrap
}  // namespace utils
