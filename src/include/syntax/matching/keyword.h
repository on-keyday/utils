/*license*/

// keyword -matching keyword
#pragma once

#include "context.h"
#include "../make_parser/keyword.h"
#include "state.h"

namespace utils {
    namespace syntax {

        template <class String, template <class...> class Vec>
        MatchState match_keyword(Context<String, Vec>& ctx, wrap::shared_ptr<Element<String, Vec>>& v, String& token) {
            auto report = [&](auto&&... args) {
                ctx.err.packln("error: ", args...);
                ctx.errat = r.get();
                ctx.errelement = v;
            };
            Reader<String>& r = ctx.r;
            if (!any(v->attr & Attribute::adjacent)) {
                r.read();
            }
            Single<String, Vec>* value = static_cast<Single<String, Vec>*>(std::addressof(*v));
            auto e = r.get();
            MatchState ret = MatchState::not_match;
            auto common_begin = [&](KeyWord kwd, auto&& f) {
                if (value->tok->has(keywordv(kwd))) {
                    if (!e) {
                        report("unexpected EOF. expect identifier.");
                        ret = MatchState::eof;
                        return true;
                    }
                    f();
                    return true;
                }
            };
            if (common_begin(KeyWord::id, [&] {
                    if (!e->is(tknz::TokenKind::identifier)) {
                        return false;
                    }
                    token = e->to_string();
                    ret = MatchState::succeed;
                    r.consume();
                })) {
                return ret;
            }
        }
    }  // namespace syntax
}  // namespace utils