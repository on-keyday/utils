/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


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

           private:
            bool is_ignore() const override {
                if (!ignore_line && this->current->is(tknz::TokenKind::line)) {
                    return false;
                }
                return this->current->has("#") || this->default_ignore(this->current);
            }

            void consume_hook(bool toroot) override {
                count++;
            }

           public:
            Reader from_current() {
                Reader ret(this->current);
                ret.ignore_line = ignore_line;
                ret.basecount = count;
                ret.count = count;
                return ret;
            }

            void seek_to(Reader& r) {
                ignore_line = r.ignore_line;
                count = r.count;
                this->current = r.current;
            }
        };
    }  // namespace syntax
}  // namespace utils
