/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <wrap/cout.h>
#include <space/line_pos.h>
#include <bnf/bnf.h>
extern utils::wrap::UtfOut& cout;
namespace igen2 {
    namespace bnf = utils::bnf;
    template <class Seq>
    struct ErrC : utils::minilang::parser::NullErrC {
        Seq* seq;
        utils::wrap::internal::Pack pack;
        bool buffer_mode = false;
        ErrC(Seq* s)
            : seq(s) {}
        void say(auto&&... msg) {
            if (buffer_mode) {
                pack.packln(msg...);
                return;
            }
            (cout << ... << msg) << "\n";
        }

        void trace(auto start, auto&& s) {
            std::string w;
            seq->rptr = s.rptr;
            utils::space::write_src_loc(w, *seq);
            if (buffer_mode) {
                pack.packln(w);
                return;
            }
            cout << w << "\n";
        }

        void node(const std::shared_ptr<bnf::minl::MinNode>& node) {
            if (seq) {
                std::string w;
                seq->rptr = node->pos.begin;
                utils::space::write_src_loc(w, *seq, node->pos.end - node->pos.begin);
                if (buffer_mode) {
                    pack.packln(w);
                    return;
                }
                cout << w << "\n";
            }
        }

        void clear() {
            pack.clear();
        }
    };
}  // namespace igen2
