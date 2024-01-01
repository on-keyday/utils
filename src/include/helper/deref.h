/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// deref - dereference wrapper
#pragma once

#include <memory>

namespace futils {
    namespace helper {
        enum class DerefKind {
            object,
            ptrlike,
            ptr,
        };

        namespace internal {

            /*
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
            */

            template <class T>
            concept has_arrow = requires(T t) {
                { t.operator->() };
            };

            template <class T>
            concept has_deref = requires(T t) {
                { std::addressof(*t) };
            };

            constexpr auto deref_impl(auto& t) {
                if constexpr (has_arrow<decltype(t)>) {
                    return t.operator->();
                }
                else if constexpr (has_deref<decltype(t)>) {
                    return std::addressof(*t);
                }
                else {
                    return std::addressof(t);
                }
            }
        }  // namespace internal

        template <class T>
        constexpr auto deref(T&& t) {
            return internal::deref_impl(t);
        }

        template <class T>
        constexpr T* deref(T* t) {
            return t;
        }

        /*
        template <class T>
        constexpr DerefKind derefkind() {
            return internal::derefable<T>::kind;
        }

        template <class T>
        constexpr bool is_deref() {
            return derefkind<T>() != DerefKind::object;
        }*/
    }  // namespace helper
}  // namespace futils
