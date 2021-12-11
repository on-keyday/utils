/*license*/

#define UTILS_SYNTAX_NO_EXTERN_SYNTAXC
#include "../../include/syntax/syntaxc/make_syntaxc.h"

namespace utils {
    namespace syntax {
        void instanciate() {
            SyntaxC<wrap::string, wrap::vector, wrap::map> instance;
        }
    }  // namespace syntax
}  // namespace utils
