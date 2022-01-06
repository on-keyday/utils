/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// deref - dereference wrapper
#pragma once

#include "sfinae.h"
#include <memory>

namespace utils {
    namespace helper {
        enum class DerefKind {
            object,
            ptrlike,
            ptr,
        };

        namespace internal {

            SFINAE_BLOCK_T_BEGIN(derefable, *std::declval<T>())
            constexpr static auto kind = std::is_pointer_v<T> ? DerefKind::ptr : DerefKind::ptrlike;
            using type = decltype(std::addressof(*std::declval<T&>()));
            constexpr static auto deref(T& t) {
                if (!t) {
                    return type(nullptr);
                }
                return std::addressof(*t);
            }
            SFINAE_BLOCK_T_ELSE(derefable)
            constexpr static auto kind = DerefKind::object;
            using type = decltype(std::addressof(std::declval<T&>()));
            constexpr static auto deref(T& t) {
                return std::addressof(t);
            }
            SFINAE_BLOCK_T_END()
        }  // namespace internal

        template <class T>
        constexpr auto deref(T&& t) {
            return internal::derefable<T>::deref(t);
        }

        template <class T>
        constexpr T* deref(T* t) {
            return t;
        }

        template <class T>
        constexpr DerefKind derefkind() {
            return internal::derefable<T>::kind;
        }

        template <class T>
        constexpr bool is_deref() {
            return derefkind<T>() != DerefKind::object;
        }
    }  // namespace helper
}  // namespace utils
