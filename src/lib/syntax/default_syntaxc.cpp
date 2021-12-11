/*license*/

#define UTILS_SYNTAX_NO_EXTERN_SYNTAXC
#include "../../include/syntax/syntaxc/make_syntaxc.h"

namespace utils {
    namespace syntax {
        void instanciate() {
            SyntaxC<wrap::string, wrap::vector, wrap::map> instance;
            utils::Sequencer input("");

            utils::tokenize::Tokenizer<utils::wrap::string, utils::wrap::vector> tokenizer;

            instance.make_tokenizer(input, tokenizer);

            auto other = "";

            utils::Sequencer input2(other);

            decltype(tokenizer)::token_t res;

            tokenizer.tokenize(input2, res);

            utils::syntax::Reader<utils::wrap::string> r{res};
            instance.matching(r);
        }
    }  // namespace syntax
}  // namespace utils
