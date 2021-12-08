/*license*/

// show_current -  show current with line
#pragma once

#include "../reader.h"
#include "../../wrap/pack.h"
#include "../cast.h"

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
                output.pack(line, "|");
                if (!beginline) {
                    output.pack("[EOF]");
                    return;
                }
                size_t offset = 0;
                size_t currentoffset = 0;
                size_t currentlen = 0;
                for (auto p = beginline; p; p = p->next) {
                    if (p->is(TokenKind::line)) {
                        break;
                    }
                    auto str = p->to_string();
                    if (p == current) {
                        currentoffset = offset;
                        currentlen = str.size();
                    }
                    offset += str.size();
                    output.pack(str);
                }
                output.packln();
                for (auto i = 0; i < currentoffset; i++) {
                    output.pack(" ");
                }
                for (auto i = 0; i < currentlen; i++) {
                    output.pack("^");
                }
                output.packln();
            }
        }  // namespace fmt
    }      // namespace tokenize
}  // namespace utils
