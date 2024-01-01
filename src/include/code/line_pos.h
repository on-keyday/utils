/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#pragma once
#include "../core/sequencer.h"
#include "../strutil/eol.h"

namespace futils {
    namespace code {

        template <class T>
        constexpr void get_linepos(Sequencer<T>& seq, size_t& line, size_t& pos) {
            auto rptr = seq.rptr;
            seq.rptr = 0;
            line = 0;
            pos = 0;
            for (; !seq.eos() && seq.rptr < rptr;) {
                if (strutil::parse_eol<true>(seq)) {
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

    }  // namespace code
}  // namespace futils
