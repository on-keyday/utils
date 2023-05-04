/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// line_pos - show line and pos
#pragma once
#include "../core/sequencer.h"
#include "../helper/appender.h"
#include "../number/to_string.h"
#include "../number/insert_space.h"
#include "../helper/pushbacker.h"
#include "eol.h"

namespace utils {
    namespace space {

        struct SrcLoc {
            size_t line = 0;
            size_t pos = 0;
            size_t next_line_pos = 0;
            bool omit_by_line = false;
        };

        template <class Out, class T>
        constexpr SrcLoc write_src_loc(Out& w, Sequencer<T>& seq, size_t poslen = 1, size_t line_limit = 1, char posc = '^', const char* show_endof = " [EOF]") {
            const size_t orig_rptr = seq.rptr;
            helper::CountPushBacker<Out&> c{w};
            size_t orig_line, orig_pos;
            get_linepos(seq, orig_line, orig_pos);
            auto line = orig_line;
            auto pos = orig_pos;
            seq.rptr = seq.rptr - orig_pos;
            size_t ptr = 0;
            size_t lpos = 0;
            bool omit_pos = false;
            for (auto i = 0; i < line_limit; i++) {
                c.count = 0;
                if (line + 1 < 1000) {
                    number::insert_space(c, 4, line + 1);
                }
                else if (line + 1 < 100000) {
                    number::insert_space(c, 7, line + 1);
                }
                number::to_string(c, line + 1);
                c.push_back('|');
                size_t line_prefix = c.count;
                size_t bol = seq.rptr;
                while (!seq.eos() && seq.current() != '\n' && seq.current() != '\r') {
                    c.push_back(seq.current());
                    seq.consume();
                }
                lpos = seq.rptr;
                lpos++;
                if (seq.current() == '\r' && seq.current(1) == '\n') {
                    lpos++;
                }
                omit_pos = false;
                if (posc != 0) {
                    seq.rptr = orig_rptr;
                    c.push_back('\n');
                    for (size_t i = 0; i < line_prefix + pos; i++) {
                        c.push_back(' ');
                    }
                    for (; ptr < poslen; ptr++) {
                        if (seq.eos()) {
                            break;
                        }
                        if (seq.current(ptr) == '\r' || seq.current(ptr) == '\n') {
                            omit_pos = true;
                            if (seq.current(ptr) == '\r' && seq.current(ptr + 1) == '\n') {
                                ptr++;
                            }
                            ptr++;
                            break;
                        }
                        c.push_back(posc);
                    }
                    if (show_endof) {
                        if (seq.eos()) {
                            c.push_back(posc);
                            append(c, show_endof);
                        }
                    }
                }
                if (!omit_pos) {
                    break;
                }
                if (i + 1 < line_limit) {
                    c.push_back('\n');
                }
                seq.rptr = lpos;
                line += 1;
                pos = 0;
            }
            seq.rptr = orig_rptr;
            return SrcLoc{
                .line = orig_line,
                .pos = orig_pos,
                .next_line_pos = lpos,
                .omit_by_line = omit_pos,
            };
        }
    }  // namespace space
}  // namespace utils
