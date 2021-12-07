/*license*/

// element - define syntax element type
#pragma once

#include "attribute.h"

namespace utils {
    namespace syntax {
        enum class SyntaxType {
            literal,
            keyword,
            reference,
            group,
        };

        template <class String>
        struct Element {
            SyntaxType type;
            Attribute attr;
        };
    }  // namespace syntax
}  // namespace utils
