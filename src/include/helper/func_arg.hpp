/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <type_traits>

namespace futils::helper {
    template <class T, size_t I>
    struct func_args;

    template <size_t I, class... As>
    struct func_next_arg {
        using type = void;
    };

    template <size_t I, class A, class... As>
    struct func_next_arg<I, A, As...> {
        using type = std::conditional_t<I == 0, A, typename func_next_arg<I - 1, As...>::type>;
    };

    // normal function pointer
    template <class R, size_t I, class... Args>
    struct func_args<R (*)(Args...), I> {
        using type = typename func_next_arg<I, Args...>::type;
    };

    // member function pointer
    template <class R, class C, size_t I, class... Args>
    struct func_args<R (C::*)(Args...), I> {
        using type = typename func_next_arg<I, Args...>::type;
    };

    template <class V>
    concept has_operator_call = requires(V v) {
        { &V::operator() };
    };

    // functor with operator()
    template <has_operator_call F, size_t I>
    struct func_args<F, I> {
        using type = typename func_args<decltype(&F::operator()), I>::type;
    };

    template <class F, size_t I = 0>
    using func_arg_t = typename func_args<F, I>::type;

    namespace test {
        struct functor {
            void operator()(int, double) {}
        };

        static_assert(std::is_same_v<func_arg_t<functor, 0>, int>);
        static_assert(std::is_same_v<func_arg_t<functor, 1>, double>);

        static_assert(std::is_same_v<func_arg_t<void (*)(int, double), 0>, int>);
        static_assert(std::is_same_v<func_arg_t<void (*)(int, double), 1>, double>);

        struct test_class {
            void member_func(int, double) {}
        };

        static_assert(std::is_same_v<func_arg_t<void (test_class::*)(int, double), 0>, int>);
        static_assert(std::is_same_v<func_arg_t<void (test_class::*)(int, double), 1>, double>);
    }  // namespace test
}  // namespace futils::helper