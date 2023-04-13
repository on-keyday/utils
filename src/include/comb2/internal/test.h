/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <type_traits>

namespace utils::comb2 {
    namespace test {
        constexpr void error_if_constexpr(auto&&... arg) {
            if (std::is_constant_evaluated()) {
                throw "error";
            }
        }

        template <class T, template <class...> class Templ>
        struct template_instance_of_t : std::false_type {};
        template <class U, template <class...> class Templ>
        struct template_instance_of_t<Templ<U>, Templ> : std::true_type {};

        template <class T, template <class...> class Templ>
        concept is_template_instance_of = template_instance_of_t<T, Templ>::value;

        template <class A>
        struct Test {};

        static_assert(is_template_instance_of<Test<int>, Test> && !is_template_instance_of<int, Test>);

    }  // namespace test
}  // namespace utils::comb2
