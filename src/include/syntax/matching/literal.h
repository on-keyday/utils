/*license*/

// literal -  match literal
#pragma once
#include "context.h"
#include "state.h"

#include <cassert>

namespace utils {
    namespace syntax {
        namespace internal {
            template <class String, template <class...> class Vec>
            MatchState match_literal(Context<String, Vec>& ctx, wrap::shared_ptr<Element<String, Vec>>& v, MatchResult<String>& result) {
                auto report = [&](auto&&... args) {
                    ctx.err.packln("error: ", args...);
                    ctx.errat = r.get();
                    ctx.errelement = v;
                };
                Single<String, Vec>* value = static_cast<Single<String, Vec>*>(std::addressof(*v));
                assert(value);
                Reader<String>& r = ctx.r;
                if (!any(value->attr & Attribute::adjacent)) {
                    r.read();
                }
                auto e = r.get();
                if (!e) {
                    report("unexpected EOF");
                    return MatchState::not_match;
                }
                if (!e->is(tknz::TokenKind::keyword) && !e->is(tknz::TokenKind::symbol)) {
                    report("expect keyword but token is ", e->what(), " (symbol `", e->to_string(), "`)");
                    return MatchState::not_match;
                }
                r.consume();
                result.kind = e->is(tknz::TokenKind::keyword) ? KeyWord::literal_keyword : KeyWord::literal_symbol;
                result.token = e->to_string();
                return MatchState::succeed;
            }
        }  // namespace internal
    }      // namespace syntax
}  // namespace utils
