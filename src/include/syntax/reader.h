/*license*/

#pragma once
// reader - customized tokenize::Reader for syntax

#include "../tokenize/reader.h"

namespace utils {
    namespace syntax {
        namespace tknz = tokenize;

        template <class String>
        struct Reader : tknz::Reader<String> {
            size_t count = 0;
            size_t basecount = 0;
            bool ignore_line = true;
            using tknz::Reader<String>::Reader;

            bool is_ignore() const override {
                if (!ignore_line && this->current->is(tknz::TokenKind::line)) {
                    return return false;
                }
                return this->current->has("#") || default_ignore(this->current);
            }

            void consume_hook() override {
                count++;
            }

            Reader from_current() {
                Reader ret(this->current);
                ret.ignore_line = ignore_line;
                ret.basecount = count;
                ret.count = count;
                return ret;
            }
        };
    }  // namespace syntax
}  // namespace utils
