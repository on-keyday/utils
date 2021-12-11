/*license*/

// syntaxc - syntax combinater
#pragma once

#include "../make_parser/parse.h"
#include "../matching/matching.h"
#include "../make_parser/tokenizer.h"
#include "../../operator/callback.h"

namespace utils {
    namespace syntax {
        struct Default {
        };

        template <class String, template <class...> class Vec, template <class...> class Map>
        struct SyntaxC {
            syntax::Match<String, Vec, Map> match;
            ops::DefaultCallback<MatchState, MatchResult<String>&> cb;

            template <class T>
            bool make_tokenizer(Sequencer<T>& input, tknz::Tokenizer<String, Vec>& output) {
                match.matcher.context.err.clear();
                wrap::shared_ptr<tknz::Token<String>> rbase;
                const char* errmsg = nullptr;
                if (!syntax::tokenize_and_merge<T, String, Vec>(input, rbase, &errmsg)) {
                    match.matcher.context.err << errmsg;
                    return false;
                }
                internal::ParseContext<String, Vec> ctx = syntax::make_parse_context<String, Vec>(rbase);
                match.matcher.segments.clear();
                if (!syntax::parse(ctx, match.matcher.segments)) {
                    ctx.err << match.matcher.context.err.pack();
                    return false;
                }
                output.keyword.predef = std::move(ctx.keyword);
                output.symbol.predef = std::move(ctx.symbol);
                return true;
            }

            bool matching(Reader<String>& r, const String& root = String()) {
                match.matcher.context.r = r;
                if (!cb) {
                    cb = [](auto&) { return MatchState::succeed; };
                }
                return match.matching(cb, root) == MatchState::succeed;
            }
        };
    }  // namespace syntax
}  // namespace utils
