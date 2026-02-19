/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <utility>
#include "iovec.h"

namespace futils::view {
    template <class A, class B>
    struct Concat {
        A a;
        B b;

        using element_type = decltype(a[1] + b[1]);

        template <class TA, class TB>
        constexpr Concat(TA&& a, TB&& b) noexcept
            : a{std::forward<TA>(a)}, b{std::forward<TB>(b)} {}

        constexpr size_t size() const noexcept(noexcept(a.size() + b.size())) {
            return a.size() + b.size();
        }

        constexpr element_type operator[](size_t i) const noexcept(noexcept(a.size() + b.size())) {
            return i < a.size() ? a[i] : b[i - a.size()];
        }
    };

    template <class A, class B>
    constexpr auto make_concat(A&& a, B&& b) noexcept {
        return Concat<A, B>{std::forward<A>(a), std::forward<B>(b)};
    }

    template <class A, class B, class C, class... R>
    constexpr auto make_concat(A&& a, B&& b, C&& c, R&&... r) noexcept {
        return make_concat(make_concat(std::forward<A>(a), std::forward<B>(b)),
                           std::forward<C>(c), std::forward<R>(r)...);
    }

    namespace test {
        constexpr auto test_concat() {
            constexpr auto conc = make_concat(view::rcvec("abc"), view::rcvec("def"), view::rcvec("ghi"), view::rcvec("jkl"));
            static_assert(conc.size() == 12);
            static_assert(conc[0] == 'a');
            static_assert(conc[1] == 'b');
            static_assert(conc[2] == 'c');
            static_assert(conc[3] == 'd');
            static_assert(conc[4] == 'e');
            static_assert(conc[5] == 'f');
            static_assert(conc[6] == 'g');
            static_assert(conc[7] == 'h');
            static_assert(conc[8] == 'i');
            static_assert(conc[9] == 'j');
            static_assert(conc[10] == 'k');
            static_assert(conc[11] == 'l');
            return true;
        }

        static_assert(test_concat());
    }  // namespace test

    template <class V>
    struct VecConcat {
        V v;

       private:
        size_t size_sum = 0;

       public:
        using element_type = typename V::value_type;

        template <class TV>
        constexpr VecConcat(TV&& v) noexcept
            : v{std::forward<TV>(v)} {
            for (auto& e : v) {
                size_sum += e.size();
            }
        }

        constexpr size_t size() const noexcept(noexcept(v.size())) {
            return size_sum;
        }

        constexpr element_type operator[](size_t i) const noexcept(noexcept(v.size())) {
            for (auto& e : v) {
                if (i < e.size()) {
                    return e[i];
                }
                i -= e.size();
            }
            if constexpr (std::is_reference_v<element_type>) {
                return *v[v.size()].end();  // may cause UB, but it's a bug in the caller
            }
            else {
                return {};
            }
        }
    };

    template <class V>
    constexpr auto make_vec_concat(V&& v) noexcept {
        return VecConcat<V>{std::forward<V>(v)};
    }
}  // namespace futils::view
