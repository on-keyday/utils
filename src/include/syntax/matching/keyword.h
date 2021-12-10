/*license*/

// keyword -matching keyword
#pragma once

#include "context.h"
#include "../make_parser/keyword.h"
#include "state.h"
#include "number.h"
#include "../../tokenize/cast.h"

namespace utils {
    namespace syntax {
        template <class String>
        struct KeywordMatchResult {
            String token;
            KeyWord kind = KeyWord::id;
        };

        template <class String, template <class...> class Vec>
        MatchState match_keyword(Context<String, Vec>& ctx, wrap::shared_ptr<Element<String, Vec>>& v, KeywordMatchResult<String>& result) {
            auto report = [&](auto&&... args) {
                ctx.err.packln("error: ", args...);
                ctx.errat = r.get();
                ctx.errelement = v;
            };
            auto fmterr = [](auto expected, auto& e) {
                report("expect ", expected, " but token is ", e->what(), "(symbol `", e->to_string(), "`)");
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
                else if (is_keyword(KeyWord::indent)) {
                    r.ignore_line = false;
                    r.read();
                    r.ignore_line = true;
                    if (r.get() && r.get()->is(tknz::TokenKind::line)) {
                        r.consume();
                    }
                    else {
                        fmterr("indent", r.get());
                        return MatchState::not_match;
                    }
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
                        fmterr(tknz::token_kind(kind), e);
                        return;
                    }
                    result.token = e->to_string();
                    result.kind = kind;
                    ret = MatchState::succeed;
                    r.consume();
                };
            };
            auto number = [&](bool integer) {
                return [&, integer] {
                    auto err = parse_float(ctx, v, result.token, integer);
                    if (err < 0) {
                        ret = MatchState::fatal;
                    }
                    else if (err) {
                        ret = MatchState::succeed;
                        if (integer) {
                            result.kind = KeyWord::integer;
                        }
                        else {
                            result.kind = KeyWord::number;
                        }
                    }
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
                    fmterr("EOF", e);
                    return ret;
                }
                return MatchState::succeed;
            }
            else if (common_begin(KeyWord::string, [&] {
                         if (!e->has("\"")) {
                             fmterr("string", e);
                             return;
                         }
                         e = r.consume_get();
                         if (!e) {
                             report("unexpected EOF");
                             ret = MatchState::eof;
                             return;
                         }
                         if (!e->is(tknz::TokenKind::comment)) {
                             fmterr("string", e);
                             return;
                         }
                         result.token = e->to_string();
                         e = r.consume_get();
                         if (!e) {
                             report("unexpected EOF");
                             ret = MatchState::eof;
                             return;
                         }
                         if (!e->has("\"")) {
                             fmterr("string", e);
                             return;
                         }
                         r.consume();
                         result.kind = KeyWord::string;
                     })) {
                return ret;
            }
            else if (common_begin(KeyWord::integer, number(true))) {
                return ret;
            }
            else if (common_begin(KeyWord::number, number(false))) {
                return ret;
            }
            else if (common_begin(KeyWord::indent, [&] {

                     })) {
                return ret;
            }
            else if (is_keyword(KeyWord::not_space)) {
                String tok;
                while (true) {
                    e = r.get();
                    if (!e || e->is(tknz::TokenKind::line) || e->is(tknz::TokenKind::space)) {
                        break;
                    }
                }
                if (tok.size() == 0) {
                    fmterr("not-space", e);
                    return MatchState::not_match;
                }
                return MatchState::succeed;
            }
        }
    }  // namespace syntax
}  // namespace utils
