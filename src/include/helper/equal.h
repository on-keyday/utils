/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// equal -  equal function
#pragma once

#include "../core/sequencer.h"
#include "compare_type.h"

namespace utils {
    namespace helper {
        template <class In, class Cmp, class Compare = decltype(default_compare())>
        constexpr bool equal(In&& in, Cmp&& cmp, Compare&& compare = default_compare()) {
            Sequencer<buffer_t<In&>> intmp(in);
            return intmp.seek_if(cmp, compare) && intmp.eos();
        }
    }  // namespace helper
}  // namespace utils
