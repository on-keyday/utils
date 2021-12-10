/*license*/

// keyword -matching keyword
#pragma once

#include "context.h"
#include "../make_parser/keyword.h"
#include "state.h"
#include "number.h"

namespace utils {
    namespace syntax {

        template <class String, template <class...> class Vec>
        MatchState match_keyword(Context<String, Vec>& ctx, wrap::shared_ptr<Element<String, Vec>>& v, String& token) {
            auto report = [&](auto&&... args) {
                ctx.err.packln("error: ", args...);
                ctx.errat = r.get();
                ctx.errelement = v;
            };
            Single<String, Vec>* value = static_cast<Single<String, Vec>*>(std::addressof(*v));
            Reader<String>& r = ctx.r;
            auto is_keyword = [&](KeyWord kwd) {
                return value->tok->has(keywordv(kwd));
            };
            if (!any(v->attr & Attribute::adjacent)) {
                if (is_keyword(KeyWord::eol)) {
                    r.ignore_line = false;
                    r.read();
                    r.ignore_line = true;
                }
                else {
                    r.read();
                }
            }
            auto e = r.get();
            MatchState ret = MatchState::not_match;
            auto common_begin = [&](KeyWord kwd, auto&& f) {
                if (is_keyword(kwd)) {
                    if (!e) {
                        report("unexpected EOF");
                        ret = MatchState::eof;
                        return true;
                    }
                    f();
                    return true;
                }
            };
            auto filter_tokenkind = [&](tknz::TokenKind kind) {
                return [&, kind] {
                    if (!e->is(kind)) {
                        report("expect ", tknz::token_kind(kind), " but token is ", e->what(), "(symbol `", e->to_string(), "`)");
                        return;
                    }
                    token = e->to_string();
                    ret = MatchState::succeed;
                    r.consume();
                };
            };
            if (common_begin(KeyWord::id, filter_tokenkind(tknz::TokenKind::identifier))) {
                return ret;
            }
            else if (common_begin(KeyWord::keyword, filter_tokenkind(tknz::TokenKind::keyword))) {
                return ret;
            }
            else if (common_begin(KeyWord::symbol, filter_tokenkind(tknz::TokenKind::symbol))) {
                return ret;
            }
            else if (common_begin(KeyWord::eol, filter_tokenkind(tknz::TokenKind::line))) {
                return ret;
            }
            else if (is_keyword(KeyWord::eof)) {
                if (e) {
                    report("expect EOF but token is", e->what(), "(symbol `", e->to_string(), "`)");
                    return ret;
                }
                return MatchState::succeed;
            }
        }
    }  // namespace syntax
}  // namespace utils
