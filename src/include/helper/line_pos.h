/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
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

        struct SrcLoc {
            size_t line;
            size_t pos;
        };

        template <class Out, class T>
        SrcLoc write_src_loc(Out& w, Sequencer<T>& seq, size_t poslen = 1, char posc = '^', const char* show_endof = " [EOF]") {
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
                for (size_t i = 0; i < poslen; i++) {
                    c.push_back(posc);
                    if (seq.current(i) == '\r' || seq.current(i) == '\n') {
                        break;
                    }
                }
                if (show_endof) {
                    if (seq.current() == '\r' || seq.current() == '\n') {
                        append(c, " [EOL]");
                    }
                    if (seq.eos()) {
                        append(c, show_endof);
                    }
                }
            }
            seq.rptr = bs;
            return SrcLoc{.line = line, .pos = pos};
        }
    }  // namespace helper
}  // namespace utils
