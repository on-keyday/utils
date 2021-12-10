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
            bool make_tokenizer(Sequencer<T>& input, tknz::Tokenizer<String, Vec>& output) {
                match.matcher.context.err.clear();
                wrap::shared_ptr<tknz::Token<String>> rbase;
                const char* errmsg = nullptr;
                if (!syntax::tokenize_and_merge(input, rbase, errmsg)) {
                    match.matcher.context.err << errmsg;
                    return false;
                }
                internal::ParseContext<String, Vec> ctx = syntax::make_parse_context<String, Vec>(rbase);
                wrap::shared_ptr<Element<String, Vec>> result;
                if (!syntax::parse(ctx, result)) {
                    ctx.err << match.matcher.context.err.pack();
                    return false;
                }

                return true;
            }
        };
    }  // namespace syntax
}  // namespace utils
