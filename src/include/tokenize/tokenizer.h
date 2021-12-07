/*license*/

// tokenizer - implementation of tokenizer
#pragma once

#include "predefined.h"

namespace utils {
    namespace tokenize {

        template <class String, template <class...> class Vec>
        struct Tokenizer {
            Predefined<String, Vec> keyword;
            Predefined<String, Vec> symbol;
        };
    }  // namespace tokenize
}  // namespace utils