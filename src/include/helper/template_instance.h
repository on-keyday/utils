/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <type_traits>
#include <tuple>

namespace utils {
    namespace helper {
        template <class T, template <class...> class Templ>
        struct template_instance_of_t : std::false_type {};

        template <size_t i, size_t cur, class T, class... U>
        struct param_at_t {
            using param = std::conditional_t<i == cur, T, param_at_t<i, cur + 1, U...>>;
        };

        template <size_t i, size_t cur, class T>
        struct param_at_t<i, cur, T> {
            using param = std::enable_if_t<i == cur, T>;
        };

        template <class T, class... U>
        struct type_params : std::true_type {
            using param = T;
            using other = type_params<U...>;
            using as_tuple = std::tuple<T, U...>;

            template <size_t i>
            using param_at = param_at_t<i, 0, T, U...>;
        };

        template <class T>
        struct type_params<T> : std::true_type {
            using param = T;
            using other = void;
            using as_tuple = std::tuple<T>;

            template <size_t i>
            using param_at = param_at_t<i, 0, T>;
        };

        template <class... U, template <class...> class Templ>
        struct template_instance_of_t<Templ<U...>, Templ> : type_params<U...> {
            using instance = Templ<U...>;
        };

        template <class T, template <class...> class Templ>
        concept is_template_instance_of = template_instance_of_t<T, Templ>::value;

        template <class A>
        struct Test {};

        static_assert(is_template_instance_of<Test<int>, Test> && !is_template_instance_of<int, Test>);

    }  // namespace helper
}  // namespace utils
