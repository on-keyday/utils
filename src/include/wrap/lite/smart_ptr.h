/*license*/

// smart_ptr - wrap default smart pointer
#pragma once

#if !defined(UTILS_WRAP_SHARED_PTR_TEMPLATE) || !defined(UTILS_WRAP_MAKE_SHARED_PTR_FUNC)
#include <memory>
#define UTILS_WRAP_SHARED_PTR_TEMPLATE std::shared_ptr
#define UTILS_WRAP_MAKE_SHARED_PTR_FUNC std::make_shared
#endif

namespace utils {
    namespace wrap {
        template <class T>
        using shared_ptr = UTILS_WRAP_SHARED_PTR_TEMPLATE<T>;

        template <class T, class... Args>
        shared_ptr<T> make_shared(Args&&... args) {
            return UTILS_WRAP_MAKE_SHARED_PTR_FUNC<T>(std::forward<Args>(args)...);
        }
    }  // namespace wrap
}  // namespace utils