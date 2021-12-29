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

        template <class In, class Cmp, class Compare = decltype(default_compare())>
        constexpr bool starts_with(In&& in, Cmp&& cmp, Compare&& compare = default_compare()) {
            Sequencer<buffer_t<In&>> intmp(in);
            return intmp.match(cmp, compare);
        }

        template <class In, class Cmp, class Compare = decltype(default_compare())>
        constexpr bool ends_with(In&& in, Cmp&& cmp, Compare&& compare = default_compare()) {
            using reverse_type = ReverseView<buffer_t<In&>>;
            Sequencer<reverse_type> intmp(reverse_type{in});
            ReverseView<buffer_t<Cmp&>> cmptmp{cmp};
            return intmp.match(cmptmp, compare);
        }

        template <class In, class Cmp, class Compare = decltype(default_compare())>
        constexpr bool equal(In&& in, Cmp&& cmp, Compare&& compare = default_compare()) {
            Sequencer<buffer_t<In&>> intmp(in);
            return intmp.seek_if(cmp, compare) && intmp.eos();
        }

        template <class In, class Begin, class End, class Compare = decltype(default_compare())>
        constexpr bool sandwiched(In&& in, Begin&& begin, End&& end, Compare&& compare = default_compare()) {
            return starts_with(in, begin, compare) && ends_with(in, end, compare);
        }

        // very simple search
        template <class In, class Cmp, class Compare = decltype(default_compare())>
        constexpr size_t find(In&& in, Cmp&& cmp, Compare&& compare = default_compare(), size_t pos = 0) {
            Sequencer<buffer_t<In&>> intmp(in);
            Buffer<buffer_t<Cmp&>> cmptmp(cmp);
            if (cmptmp.size() == 0) {
                return ~0;
            }
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
            return find(in, cmp) != ~0;
        }

        template <bool no_zero = false, class String, class Validate>
        bool is_valid(String&& str, Validate&& validate) {
            auto seq = make_ref_seq(str);
            bool first = true;
            while (!seq.eos()) {
                if (!validate(seq.current())) {
                    return false;
                }
                seq.consume();
                first = false;
            }
            if constexpr (no_zero) {
                return first == false;
            }
            return true;
        }

        template <class T, class Sep = const char*>
        struct SplitView {
            Buffer<buffer_t<T>> buf;
            Sep sep;
            constexpr SplitView(T&& t)
                : buf(std::forward<T>(t)) {}

            constexpr auto operator[](size_t index) const {
                size_t first = index == 0 ? 0 : find(buf.buffer, sep, default_compare(), index - 1);
                size_t second = find(buf.buffer, sep, default_compare(), index);
                if (first == ~0 || second == ~0) {
                    return make_ref_slice(buf.buffer, 0, 0);
                }
                return make_ref_seq(buf.buffer, index == 0 ? first : first + 1, second);
            }
        };

    }  // namespace helper
}  // namespace utils
