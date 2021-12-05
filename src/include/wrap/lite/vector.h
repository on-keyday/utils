/*license*/

// vector - wrap default vector template
#pragma once

#if !defined(UTILS_WRAP_VECTOR_TEMPLATE)
#include <vector>
#define UTILS_WRAP_VECTOR_TEMPLATE std::vector
#endif

namespace utils {
    namespace wrap {
        template <class T>
        using vector = std::vector<T>;
    }
}  // namespace utils
