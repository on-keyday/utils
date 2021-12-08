/*license*/

// show_current -  show current with line
#pragma once

#include "../reader.h"
#include "../../wrap/pack.h"
#include "../cast.h"

#include <iomanip>

namespace utils {
    namespace tokenize {
        namespace fmt {
            template <class String>
            void show_current(Reader<String>& r, wrap::internal::Pack& output) {
                auto root = r.root;
                auto current = r.current;
                auto beginline = root;
                size_t line = 1;
                for (auto p = root; p && p != current; p = p->next) {
                    if (p->is(TokenKind::line)) {
                        auto linetok = cast<Line>(p);
                        assert(linetok);
                        line += linetok->count;
                        beginline = p->next;
                    }
                }
                auto shift = 0;
                if (line < 1000) {
                    shift = 4;
                }
                else if (line < 100000) {
                    shift = 7;
                }
                output.pack(std::setw(shift), line, "|");
                size_t offset = 0;
                size_t currentoffset = 0;
                size_t currentlen = 0;
                auto p = beginline;
                for (; p; p = p->next) {
                    if (p->is(TokenKind::line)) {
                        break;
                    }
                    auto str = p->to_string();
                    offset += str.size();
                    if (p == current) {
                        currentoffset = offset;
                        currentlen = str.size();
                    }
                    output.pack(str);
                }
                if (!p) {
                    currentoffset = offset + 1;
                    currentlen = 1;
                    //output.pack("[EOF]");
                }
                output.packln();
                for (auto i = 0; i < currentoffset + shift; i++) {
                    output.pack(" ");
                }
                for (auto i = 0; i < currentlen; i++) {
                    output.pack("^");
                }
                if (!p) {
                    output.pack(" EOF");
                }
                output.packln();
            }
        }  // namespace fmt
    }      // namespace tokenize
}  // namespace utils
