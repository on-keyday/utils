/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// strutil - string utility
#pragma once

#include "../core/sequencer.h"

#include "view.h"

#include "compare_type.h"

namespace utils {
    namespace helper {

        template <class In, class Cmp, class Compare = compare_type<In, Cmp>>
        constexpr bool starts_with(In&& in, Cmp&& cmp, Compare&& compare = default_compare<In, Cmp>()) {
            Sequencer<buffer_t<In&>> intmp(in);
            return intmp.match(cmp, compare);
        }

        template <class In, class Cmp, class Compare = compare_type<In, Cmp>>
        constexpr bool ends_with(In&& in, Cmp&& cmp, Compare&& compare = default_compare<In, Cmp>()) {
            using reverse_type = ReverseView<buffer_t<In&>>;
            Sequencer<reverse_type> intmp(reverse_type{in});
            ReverseView<buffer_t<Cmp&>> cmptmp{cmp};
            return intmp.match(cmptmp, compare);
        }

        template <class In, class Cmp, class Compare = compare_type<In, Cmp>>
        constexpr bool equal(In&& in, Cmp&& cmp, Compare&& compare = default_compare<In, Cmp>()) {
            Sequencer<buffer_t<In&>> intmp(in);
            return intmp.seek_if(cmp, compare) && intmp.eos();
        }

        template <class In, class Begin, class End, class Compare1 = compare_type<In, Begin>, class Compare2 = compare_type<In, End>>
        constexpr bool sandwiched(In&& in, Begin&& begin, End&& end, Compare1&& compare1 = default_compare<In, Begin>(), Compare2&& compare2 = default_compare<In, End>()) {
            return starts_with(in, begin, compare1) && ends_with(in, end, compare2);
        }

        // very simple search
        template <class In, class Cmp, class Compare = compare_type<In, Cmp>>
        constexpr size_t find(In&& in, Cmp&& cmp, Compare&& compare = default_compare<In, Cmp>()) {
            Sequencer<buffer_t<In&>> intmp(in);
            Buffer<buffer_t<Cmp&>> cmptmp(cmp);
            if (cmptmp.size() == 0) {
                return ~0;
            }
            while (!intmp.eos()) {
                if (intmp.remain() < cmptmp.size()) {
                    return ~0;
                }
                if (intmp.match(cmp, compare)) {
                    return intmp.rptr;
                }
                intmp.consume();
            }
            return ~0;
        }

        template <class In, class Cmp, class Compare = compare_type<In, Cmp>>
        constexpr bool contains(In&& in, Cmp&& cmp, Compare&& compare = default_compare<In, Cmp>()) {
            return find(in, cmp) != ~0;
        }

    }  // namespace helper
}  // namespace utils
