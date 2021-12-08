/*license*/

#include "../../tokenize/tokenizer.h"
#include "keyword.h"
#include "attribute.h"

namespace utils {
    namespace syntax {
        namespace tknz = tokenize;

        template <class String, template <class...> class Vec = wrap::vector>
        tknz::Tokenizer<String, Vec> make_tokenizer() {
            tknz::Tokenizer<String, Vec> ret;
            for (auto i = 0; i < sizeof(keyword_str) / sizeof(keyword_str[0]); i++) {
            }
        }
    }  // namespace syntax
}  // namespace utils