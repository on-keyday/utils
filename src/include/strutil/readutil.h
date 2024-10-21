/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// readutil - read utility
#pragma once

#include "compare_type.h"
#include "../core/sequencer.h"

namespace futils {
    namespace strutil {
        template <bool consume = true, class Header, class T, class Cmp, class Compare = decltype(default_compare())>
        constexpr bool read_until(Header& result, Sequencer<T>& seq, Cmp&& cmp, Compare&& compare = default_compare()) {
            while (!seq.eos()) {
                if (auto n = seq.match_n(cmp, compare)) {
                    if (consume) {
                        seq.consume(n);
                    }
                    return true;
                }
                result.push_back(seq.current());
                seq.consume();
            }
            return false;
        }

        template <bool empty_is_error = false, class Header, class T, class Func>
        constexpr bool read_whilef(Header& result, Sequencer<T>& seq, Func&& f) {
            bool first = true;
            while (!seq.eos()) {
                if (f(seq.current())) {
                    result.push_back(seq.current());
                    seq.consume();
                    first = false;
                    continue;
                }
                break;
            }
            if constexpr (empty_is_error) {
                return first != true;
            }
            return true;
        }

        template <class Header, class T>
        constexpr bool read_all(Header& result, Sequencer<T>& seq) {
            return read_whilef(result, seq, no_check());
        }

        template <class Header, class T, class Func = decltype(no_check())>
        constexpr bool read_n(Header& result, Sequencer<T>& seq, size_t n, Func&& validate = no_check()) {
            if (seq.remain() < n) {
                return false;
            }
            for (size_t i = 0; i < n; i++) {
                if (!validate(seq.current())) {
                    return false;
                }
                result.push_back(seq.current());
                seq.consume();
            }
            return true;
        }

        template <bool nozero = false, class Header, class T, class Cmp, class Compare = decltype(default_compare())>
        constexpr bool read_while(Header& result, Sequencer<T>& seq, Cmp&& cmp, Compare&& compare = default_compare()) {
            bool first = true;
            while (!seq.eos()) {
                if (auto n = seq.match_n(cmp, compare)) {
                    first = false;
                    for (size_t i = 0; i < n; i++) {
                        result.push_back(seq.current());
                        seq.consume();
                    }
                    continue;
                }
                break;
            }
            if constexpr (nozero) {
                return first != true;
            }
            return true;
        }

        template <class T, class Header, class Else, class Func>
        constexpr bool append_if(Header& result, Else& els, Sequencer<T>& seq, Func&& func) {
            bool allmatch = true;
            while (!seq.eos()) {
                if (func(seq.current())) {
                    result.push_back(seq.current());
                }
                else {
                    els.push_back(seq.current());
                    allmatch = false;
                }
                seq.consume();
            }
            return allmatch;
        }

    }  // namespace strutil
}  // namespace futils
