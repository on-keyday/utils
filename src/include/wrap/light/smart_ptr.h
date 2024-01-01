/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// smart_ptr - wrap default smart pointer
#pragma once

#if !defined(FUTILS_WRAP_UNIQUE_PTR_TEMPLATE) || !defined(FUTILS_WRAP_MAKE_UNIQUE_PTR_FUNC) || !defined(FUTILS_WRAP_SHARED_PTR_TEMPLATE) || !defined(FUTILS_WRAP_MAKE_SHARED_PTR_FUNC) || !defined(FUTILS_WRAP_WEAK_PTR_TEMPLATE)
#include <memory>
#define FUTILS_WRAP_SHARED_PTR_TEMPLATE std::shared_ptr
#define FUTILS_WRAP_UNIQUE_PTR_TEMPLATE std::unique_ptr
#define FUTILS_WRAP_MAKE_SHARED_PTR_FUNC std::make_shared
#define FUTILS_WRAP_MAKE_UNIQUE_PTR_FUNC std::make_unique
#define FUTILS_WRAP_WEAK_PTR_TEMPLATE std::weak_ptr
#endif

namespace futils {
    namespace wrap {
        template <class T>
        using shared_ptr = FUTILS_WRAP_SHARED_PTR_TEMPLATE<T>;

        template <class T>
        using unique_ptr = FUTILS_WRAP_UNIQUE_PTR_TEMPLATE<T>;

        template <class T>
        using weak_ptr = FUTILS_WRAP_WEAK_PTR_TEMPLATE<T>;

        template <class T, class... Args>
        shared_ptr<T> make_shared(Args&&... args) {
            return FUTILS_WRAP_MAKE_SHARED_PTR_FUNC<T>(std::forward<Args>(args)...);
        }

        template <class T, class... Args>
        unique_ptr<T> make_unique(Args&&... args) {
            return FUTILS_WRAP_MAKE_UNIQUE_PTR_FUNC<T>(std::forward<Args>(args)...);
        }
    }  // namespace wrap
}  // namespace futils
