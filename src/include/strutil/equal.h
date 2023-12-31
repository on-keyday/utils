/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// equal -  equal function
#pragma once

#include "../core/sequencer.h"
#include "compare_type.h"

namespace utils {
    namespace strutil {

        template <class In, class Cmp, class Compare = decltype(default_compare())>
        constexpr bool equal(In&& in, Cmp&& cmp, Compare&& compare = default_compare()) {
            Sequencer<buffer_t<In&>> intmp(in);
            Buffer<buffer_t<Cmp&>> cmptmp(cmp);
            return intmp.size() == cmptmp.size() && intmp.seek_if(cmp, compare) && intmp.eos();
        }

        template <class T, class U>
        concept has_equal = requires(T t, U u) {
            { t == u };
        } && (!std::is_pointer_v<std::decay_t<T>> || !std::is_pointer_v<std::decay_t<U>>);

        template <class T, class U>
        constexpr bool default_equal(T&& t, U&& u) {
            if constexpr (has_equal<T, U>) {
                return t == u;
            }
            else {
                return strutil::equal(t, u);
            }
        }
    }  // namespace strutil
}  // namespace utils
