/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// append_charsize - decide char size of appender
#pragma once
// #include "sfinae.h"
#include <type_traits>

namespace utils {
    namespace strutil {
        namespace internal {

            template <class T>
            concept has_char_type = requires {
                typename T::char_type;
            };

            template <class T>
            concept has_value_type = requires {
                typename T::value_type;
            };

            template <class T>
            concept has_index = requires(T t) {
                { t[1] };
            };

            template <class T>
            struct PBSize {
                using type = void;
            };

            template <class T, class Ret, class Arg>
            struct PBSize<Ret (T::*)(Arg)> {
                using type = Arg;
            };

            template <class T>
            concept has_pbsize =
                std::is_same_v<void, typename PBSize<decltype(T::push_back)>::type> == false;

            template <class T>
            constexpr auto append_typeof() {
                using T_ = std::decay_t<T>;
                if constexpr (has_pbsize<T_>) {
                    using V = typename PBSize<decltype(T_::push_back)>::type;
                    return V();
                }
                else if constexpr (has_char_type<T_>) {
                    using V = typename T_::char_type;
                    return V();
                }
                else if constexpr (has_value_type<T_>) {
                    using V = typename T_::value_type;
                    return V();
                }
                else if constexpr (has_index<T_>) {
                    using V = std::decay_t<decltype(std::declval<T_>()[1])>;
                    return V();
                }
                else {
                    return (void)0;
                }
            }

            template <class T>
            constexpr size_t append_sizeof() {
                using V = decltype(append_typeof<T>());
                if constexpr (std::is_same_v<V, void>) {
                    return ~0;
                }
                else {
                    return sizeof(V);
                }
            }
        }  // namespace internal

        template <class T>
        constexpr size_t append_size() {
            return internal::append_sizeof<T>();
        }

        template <class T>
        constexpr size_t append_size_v = append_size<T>();

        template <class T>
        using append_size_t = decltype(internal::append_typeof<T>());

        template <class T>
        constexpr bool is_utf_convertable = append_size_v<T> <= 4;

    }  // namespace strutil
}  // namespace utils
