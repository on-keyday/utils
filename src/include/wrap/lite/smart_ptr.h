/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// smart_ptr - wrap default smart pointer
#pragma once

#if !defined(UTILS_WRAP_SHARED_PTR_TEMPLATE) || !defined(UTILS_WRAP_MAKE_SHARED_PTR_FUNC) || !defined(UTILS_WRAP_WEAK_PTR_TEMPLATE)
#include <memory>
#define UTILS_WRAP_SHARED_PTR_TEMPLATE std::shared_ptr
#define UTILS_WRAP_MAKE_SHARED_PTR_FUNC std::make_shared
#define UTILS_WRAP_WEAK_PTR_TEMPLATE std::weak_ptr
#endif

namespace utils {
    namespace wrap {
        template <class T>
        using shared_ptr = UTILS_WRAP_SHARED_PTR_TEMPLATE<T>;

        template <class T>
        using weak_ptr = UTILS_WRAP_WEAK_PTR_TEMPLATE<T>;

        template <class T, class... Args>
        shared_ptr<T> make_shared(Args&&... args) {
            return UTILS_WRAP_MAKE_SHARED_PTR_FUNC<T>(std::forward<Args>(args)...);
        }
    }  // namespace wrap
}  // namespace utils