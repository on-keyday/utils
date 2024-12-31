/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <core/sequencer.h>

namespace futils::view {
    template <class T, class F>
    struct Transform {
        T t;
        F f;

        using element_type = decltype(f(t[0]));

        template <class TT, class FF>
        constexpr Transform(TT&& t, FF&& f) noexcept
            : t{std::forward<TT>(t)}, f{std::forward<FF>(f)} {}

        constexpr size_t size() const noexcept(noexcept(make_ref_seq(t).size())) {
            return make_ref_seq(t).size();
        }

        constexpr element_type operator[](size_t i) const noexcept(noexcept(f(t[i]))) {
            return f(t[i]);
        }
    };

    template <class T, class F>
    constexpr auto make_transform(T&& t, F&& f) noexcept {
        return Transform<T, F>{std::forward<decltype(t)>(t), std::forward<decltype(f)>(f)};
    }
}  // namespace futils::view
