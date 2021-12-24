/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// readutil - read utility
#pragma once

#include "compare_type.h"

namespace utils {
    namespace helper {
        template <bool consume = true, class Result, class T, class Cmp, class Compare = compare_type<std::remove_reference_t<T>, Cmp>>
        constexpr bool read_until(Result& result, Sequencer<T>& seq, Cmp&& cmp, Compare&& compare = default_compare<std::remove_reference_t<T>, Cmp>()) {
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

        template <class Result, class T, class Func>
        constexpr bool read_if(Result& result, Sequencer<T>& seq, Func&& cmp) {
            while (!seq.eos()) {
                if (cmp(seq.current())) {
                    result.push_back(seq.current());
                    seq.consume();
                    continue;
                }
                break;
            }
            return true;
        }

        constexpr auto no_check() {
            return [](auto) {
                return true;
            };
        }

        template <class Result, class T, class Func>
        constexpr bool read_all(Result& result, Sequencer<T>& seq) {
            return read_if(result, seq, no_check());
        }

        template <class Result, class T, class Cmp, class Compare = compare_type<std::remove_reference_t<T>, Cmp>>
        constexpr bool read_while(Result& result, Sequencer<T>& seq, Cmp&& cmp, Compare&& compare = default_compare<std::remove_reference_t<T>, Cmp>()) {
            while (!seq.eos()) {
                if (auto n = seq.match_n(cmp, compare)) {
                    for (size_t i = 0; i < n; i++) {
                        result.push_back(seq.current());
                        seq.consume();
                    }
                    continue;
                }
                break;
            }
            return true;
        }

        template <bool consume = true, class T>
        constexpr size_t match_eol(Sequencer<T>& seq) {
            if (seq.match("\r\n")) {
                if constexpr (consume) {
                    seq.consume(2);
                }
                return 2;
            }
            else if (seq.current() == '\n' || seq.current() == '\r') {
                if constexpr (consume) {
                    seq.consume(1);
                }
                return 1;
            }
            return 0;
        }
    }  // namespace helper
}  // namespace utils