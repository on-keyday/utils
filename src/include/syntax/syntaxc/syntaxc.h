/*license*/

// syntaxc - syntax combinater
#pragma once

#include "../make_parser/parse.h"
#include "../matching/matching.h"
#include "../make_parser/tokenizer.h"

namespace utils {
    namespace syntax {
        template <class String, template <class...> class Vec, template <class...> class Map>
        struct SyntaxC {
            syntax::Match<String, Vec, Map> match;

            template <class T>
            bool make_parser(Sequencer<T>& input) {
                wrap::shared_ptr<tknz::Token<String>> output;
                const char* errmsg = nullptr;
                if (!syntax::tokenize_and_merge(input, output, errmsg)) {
                }

                auto ctx = syntax::make_parse_context<String, Vec>();
                wrap::shared_ptr<Element<String, Vec>> result;
                syntax::parse(ctx, result);
            }
        };
    }  // namespace syntax
}  // namespace utils
