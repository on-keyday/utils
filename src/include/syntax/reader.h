/*license*/

#pragma once
// reader - customized tokenize::Reader for syntax

#include "../tokenize/reader.h"

namespace utils {
    namespace syntax {
        template <class String>
        struct Reader : tokenize::Reader<String> {
            size_t count = 0;
            size_t basecount = 0;
            using tokenize::Reader<String>::Reader;

            bool is_ignore() const override {
                return current->has("#") || default_ignore(this->current);
            }

            void consume_hook() override {
                count++;
            }

            Reader from_current() {
                Reader ret(this->current);
                ret.basecount = count;
                ret.count = count;
                return ret;
            }
        };
    }  // namespace syntax
}  // namespace utils
