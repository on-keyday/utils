/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// line_pos - show line and pos
#pragma once
#include "../core/sequencer.h"
#include "appender.h"
#include "readutil.h"
#include "../number/to_string.h"
#include "../number/insert_space.h"
#include "pushbacker.h"

namespace utils {
    namespace helper {
        template <class Out, class T>
        void write_src_loc(Out& w, Sequencer<T>& seq, char posc = '^', bool show_endof = true) {
            CountPushBacker<Out&> c{w};
            size_t line, pos;
            get_linepos(seq, line, pos);
            if (line + 1 < 1000) {
                number::insert_space(c, 4, line + 1);
            }
            else if (line + 1 < 100000) {
                number::insert_space(c, 7, line + 1);
            }
            number::to_string(c, line + 1);
            c.push_back('|');
            size_t bline = c.count;
            size_t bs = seq.rptr;
            seq.rptr = seq.rptr - pos;
            while (seq.current() == '\r' || seq.current() == '\n') {
                seq.consume();
            }
            size_t bptr = seq.rptr;
            while (!seq.eos() && seq.current() != '\n' && seq.current() != '\r') {
                c.push_back(seq.current());
                seq.consume();
            }
            if (posc != 0) {
                seq.rptr = bptr;
                c.push_back('\n');
                for (size_t i = 0; i < bline + pos; i++) {
                    c.push_back(' ');
                }
                c.push_back(posc);
                if (show_endof) {
                    if (seq.current() == '\r' || seq.current() == '\n') {
                        append(c, " [EOL]");
                    }
                    if (seq.eos()) {
                        append(c, " [EOS]");
                    }
                }
            }
            seq.rptr = bs;
        }
    }  // namespace helper
}  // namespace utils
