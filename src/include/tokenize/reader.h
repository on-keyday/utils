/*license*/

// reader - token reader
#pragma once

#include "token.h"

namespace utils {
    namespace tokenize {
        template <class String>
        struct Reader {
            wrap::shared_ptr<Token<String>> root;
            wrap::shared_ptr<Token<String>> current;

            constexpr Reader() {}
            constexpr Reader(wrap::shared_ptr<Token<String>>& v)
                : root(v), current(v) {}
        };
    }  // namespace tokenize
}  // namespace utils