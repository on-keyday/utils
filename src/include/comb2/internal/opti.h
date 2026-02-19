/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <type_traits>
#include <utility>

namespace futils::comb2::opti {
    template <class A, class B,
              bool = std::is_empty_v<A> && std::is_default_constructible_v<A>,
              bool = std::is_empty_v<B> && std::is_default_constructible_v<B>>
    struct MaybeEmptyAB {
        A a;
        B b;

        constexpr MaybeEmptyAB(auto&& a_, auto&& b_)
            : a(std::forward<decltype(a_)>(a_)), b(std::forward<decltype(b_)>(b_)) {}

        constexpr const A& useA() const {
            return a;
        }

        constexpr A& useA() {
            return a;
        }

        constexpr const B& useB() const {
            return b;
        }

        constexpr B& useB() {
            return b;
        }
    };

    template <class A, class B>
    struct MaybeEmptyAB<A, B, true, false> {
        B b;

        constexpr MaybeEmptyAB(auto&& a_, auto&& b_)
            : b(std::forward<decltype(b_)>(b_)) {}
        constexpr A useA() const {
            return A{};
        }

        constexpr const B& useB() const {
            return b;
        }

        constexpr B& useB() {
            return b;
        }
    };

    template <class A, class B>
    struct MaybeEmptyAB<A, B, false, true> {
        A a;

        constexpr MaybeEmptyAB(auto&& a_, auto&& b_)
            : a(std::forward<decltype(a_)>(a_)) {}

        constexpr const A& useA() const {
            return a;
        }

        constexpr A& useA() {
            return a;
        }

        constexpr B useB() const {
            return B{};
        }
    };

    template <class A, class B>
    struct MaybeEmptyAB<A, B, true, true> {
        constexpr MaybeEmptyAB(auto&& a_, auto&& b_) {}

        constexpr A useA() const {
            return A{};
        }

        constexpr B useB() const {
            return B{};
        }
    };

    template <class A, bool = std::is_empty_v<A> && std::is_default_constructible_v<A>>
    struct MaybeEmptyA {
        A a;

        constexpr MaybeEmptyA(auto&& a_)
            : a(std::forward<decltype(a_)>(a_)) {}
        constexpr const A& useA() const {
            return a;
        }

        constexpr A& useA() {
            return a;
        }
    };

    template <class A>
    struct MaybeEmptyA<A, true> {
        constexpr MaybeEmptyA(auto&& a_) {}

        constexpr A useA() const {
            return A{};
        }
    };

}  // namespace futils::comb2::opti
