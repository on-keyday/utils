/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/platform/windows/dllexport_source.h"
#define UTILS_SYNTAX_NO_EXTERN_SYNTAXC
#include "../../include/syntax/syntaxc/make_syntaxc.h"
#include "../../include/syntax/dispatcher/default_dispatcher.h"

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
            instance.cb = DefaultDispatcher{};
            instance.matching(r);
            instance.error();
        }
    }  // namespace syntax
}  // namespace utils
