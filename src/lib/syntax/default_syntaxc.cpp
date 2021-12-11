/*license*/

#pragma once

#define UTILS_SYNTAX_NO_EXTERN_SYNTAXC
#include "../../include/syntax/syntaxc/make_syntaxc.h"

namespace utils {
    namespace syntax {
        void instanciate() {
            make_syntaxc();
        }
    }  // namespace syntax
}  // namespace utils
