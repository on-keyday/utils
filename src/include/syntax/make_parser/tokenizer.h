/*license*/

#include "../../tokenize/tokenizer.h"

namespace utils {
    namespace syntax {
        namespace tknz = tokenize;

        template <class String, template <class...> class Vec = wrap::vector>
        tknz::Tokenizer<String, Vec> make_tokenizer() {
            tknz::Tokenizer<String, Vec>
        }
    }  // namespace syntax
}  // namespace utils