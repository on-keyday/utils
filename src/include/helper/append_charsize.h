/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// append_charsize - decide char size of appender
#pragma once
#include "sfinae.h"
#include <type_traits>

namespace utils {
    namespace helper {
        namespace internal {

            SFINAE_BLOCK_T_BEGIN(has_subscript, std::declval<T>()[0])
            using char_type = std::remove_cvref_t<decltype(std::declval<T>()[0])>;
            constexpr static size_t size() {
                return sizeof(std::declval<T>()[0]);
            }
            SFINAE_BLOCK_T_ELSE(has_subscript)
            constexpr static size_t size() {
                return ~0;
            }
            SFINAE_BLOCK_T_END()

            SFINAE_BLOCK_T_BEGIN(append_size, sizeof(typename T::char_type))
            using char_type = typename T::char_type;
            constexpr static size_t size() {
                return sizeof(typename T::char_type);
            }
            SFINAE_BLOCK_T_ELSE(append_size)
            using char_type = typename has_subscript<T>::char_type;
            constexpr static size_t size() {
                return has_subscript<T>::size();
            }
            SFINAE_BLOCK_T_END()
        }  // namespace internal

        template <class T>
        constexpr size_t append_size() {
            return internal::append_size<T>::size();
        }

        template <class T>
        constexpr size_t append_size_v = append_size<T>();

        template <class T>
        using append_size_t = typename internal::append_size<T>::char_type;

        template <class T>
        constexpr bool is_utf_convertable = append_size_v<T> <= 4;

    }  // namespace helper
}  // namespace utils
