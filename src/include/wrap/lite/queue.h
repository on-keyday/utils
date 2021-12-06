/*license*/

// queue - wrap default queue template
#pragma once

#if !defined(UTILS_WRAP_QUEUE_TEMPLATE)
#include <deque>
#define UTILS_WRAP_QUEUE_TEMPLATE std::deque
#endif

namespace utils {
    namespace wrap {
        template <class T>
        using queue = UTILS_WRAP_QUEUE_TEMPLATE<T>;
    }
}  // namespace utils
