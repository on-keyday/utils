/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#pragma once
#include "../core/sequencer.h"

namespace utils {
    namespace space {

        template <bool consume = true, class T>
        constexpr size_t parse_eol(Sequencer<T>& seq) {
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

        template <class T>
        constexpr void get_linepos(Sequencer<T>& seq, size_t& line, size_t& pos) {
            auto rptr = seq.rptr;
            seq.rptr = 0;
            line = 0;
            pos = 0;
            for (; seq.rptr < rptr;) {
                if (parse_eol<true>(seq)) {
                    pos = 0;
                    line++;
                }
                else {
                    seq.consume();
                    pos++;
                }
            }
            seq.rptr = rptr;
        }

    }  // namespace space
}  // namespace utils
