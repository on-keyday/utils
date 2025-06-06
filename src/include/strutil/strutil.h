/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// strutil - string utility
#pragma once

#include "../core/sequencer.h"

#include "../view/reverse.h"

#include "compare_type.h"

namespace futils {
    namespace strutil {

        template <class In, class Cmp, class Compare = decltype(default_compare())>
        constexpr bool starts_with(In&& in, Cmp&& cmp, Compare&& compare = default_compare()) {
            Sequencer<buffer_t<In&>> intmp(in);
            return intmp.match(cmp, compare);
        }

        template <class In, class Cmp, class Compare = decltype(default_compare())>
        constexpr bool ends_with(In&& in, Cmp&& cmp, Compare&& compare = default_compare()) {
            using reverse_type = view::ReverseView<buffer_t<In&>>;
            Sequencer<reverse_type> intmp(reverse_type{in});
            view::ReverseView<buffer_t<Cmp&>> cmptmp{cmp};
            return intmp.match(cmptmp, compare);
        }

        template <class In, class Begin, class End, class Compare = decltype(default_compare())>
        constexpr bool sandwiched(In&& in, Begin&& begin, End&& end, Compare&& compare = default_compare()) {
            return starts_with(in, begin, compare) && ends_with(in, end, compare);
        }

        // very simple search
        template <class In, class Cmp, class Compare = decltype(default_compare())>
        constexpr size_t find(In&& in, Cmp&& cmp, size_t pos = 0, size_t offset = 0, Compare&& compare = default_compare()) {
            Sequencer<buffer_t<In&>> intmp(in);
            Buffer<buffer_t<Cmp&>> cmptmp(cmp);
            if (cmptmp.size() == 0) {
                return ~0;
            }
            intmp.rptr = offset;
            size_t count = 0;
            while (!intmp.eos()) {
                if (intmp.remain() < cmptmp.size()) {
                    return ~0;
                }
                if (intmp.match(cmp, compare)) {
                    if (pos == count) {
                        return intmp.rptr;
                    }
                    count++;
                }
                intmp.consume();
            }
            return ~0;
        }

        template <class In, class Cmp, class Compare = decltype(default_compare())>
        constexpr bool contains(In&& in, Cmp&& cmp, Compare&& compare = default_compare()) {
            return find(in, cmp, 0, 0, compare) != ~size_t(0);
        }

        template <class In, class Cmp, class Before, class Compare = decltype(default_compare())>
        constexpr bool contains_before(In&& in, Cmp&& cmp, Before&& before, Compare&& compare = default_compare()) {
            auto target = find(in, cmp, 0, 0, compare);
            auto bef = find(in, before, 0, 0, compare);
            return target != ~0 && target < bef;
        }

        template <class String, class Validator>
        constexpr bool validate(String&& str, bool allow_empty, Validator&& validator) {
            auto seq = make_ref_seq(str);
            bool first = true;
            while (!seq.eos()) {
                if (!validator(seq.current())) {
                    return false;
                }
                seq.consume();
                first = false;
            }
            if (!allow_empty) {
                return first == false;
            }
            return true;
        }

        template <class In, class Cmp, class Compare = decltype(default_compare())>
        constexpr size_t count(In&& in, Cmp&& cmp, Compare&& compare = default_compare()) {
            auto seq = make_ref_seq(in);
            size_t count = 0;
            while (!seq.eos()) {
                if (seq.seek_if(cmp, compare)) {
                    count++;
                }
                else {
                    seq.consume();
                }
            }
            return count;
        }

    }  // namespace strutil
}  // namespace futils
