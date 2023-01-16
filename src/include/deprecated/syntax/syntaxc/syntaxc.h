/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// syntaxc - syntax combinater
#pragma once

#include "../make_parser/parse.h"
#include "../matching/matching.h"
#include "../make_parser/tokenizer.h"
#include "../operator/callback.h"

namespace utils {
    namespace syntax {

        template <class String, template <class...> class Vec, template <class...> class Map>
        struct SyntaxC {
           private:
            syntax::Match<String, Vec, Map> match;

           public:
            ops::DefaultCallback<MatchState, MatchContext<String, Vec>&> cb;

            template <class T>
            bool make_tokenizer(Sequencer<T>& input, tknz::Tokenizer<String, Vec>& output) {
                match.matcher.context.err.clear();
                wrap::shared_ptr<tknz::Token<String>> rbase;
                const char* errmsg = nullptr;
                if (!syntax::internal::tokenize_and_merge<T, String, Vec>(input, rbase, &errmsg)) {
                    match.matcher.context.err << errmsg;
                    return false;
                }
                internal::ParseContext<String, Vec> ctx = syntax::make_parse_context<String, Vec>(rbase);
                match.matcher.segments.clear();
                if (!syntax::parse<String, Vec, Map>(ctx, match.matcher.segments)) {
                    match.matcher.context.err << ctx.err.pack();
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
                auto res = match.matching(cb, root);
                r.seek_to(match.matcher.context.r);
                return res == MatchState::succeed;
            }

            wrap::internal::Pack&& error() {
                return match.matcher.context.err.pack();
            }
        };
    }  // namespace syntax
}  // namespace utils
