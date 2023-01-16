/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// keyword_literal -matching keyword and literal
#pragma once

#include "context.h"
#include "../make_parser/keyword.h"
#include "state.h"
#include "number.h"

#include <cassert>

namespace utils {
    namespace syntax {
        namespace internal {
            template <class String, template <class...> class Vec>
            MatchState match_keyword(ErrorContext<String, Vec>& ctx, wrap::shared_ptr<Element<String, Vec>>& v, MatchResult<String>& result) {
                auto report = [&](auto&&... args) {
                    ctx.err.packln("error: ", args...);
                    ctx.errat = ctx.r.get();
                    ctx.errelement = v;
                };
                auto fmterr = [&](auto expected, auto& e) {
                    report("expect ", expected, " but token is ", e ? e->what() : "EOF", "(symbol `", e ? e->to_string() : "", "`)");
                };
                assert(v);
                Single<String, Vec>* value = static_cast<Single<String, Vec>*>(std::addressof(*v));
                Reader<String>& r = ctx.r;
                auto is_keyword = [&](KeyWord kwd) {
                    return value->tok->has(keywordv(kwd));
                };
                if (is_keyword(KeyWord::bos)) {
                    result.kind = KeyWord::bos;
                    return MatchState::succeed;
                }
                else if (is_keyword(KeyWord::eos)) {
                    result.kind = KeyWord::eos;
                    return MatchState::succeed;
                }
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
                KeyWord keyword_cache;
                auto common_begin = [&](KeyWord kwd, auto&& f) {
                    if (is_keyword(kwd)) {
                        if (!e) {
                            report("unexpected EOF");
                            ret = MatchState::not_match;
                            return true;
                        }
                        keyword_cache = kwd;
                        f();
                        return true;
                    }
                    return false;
                };
                auto filter_tokenkind = [&](tknz::TokenKind kind) {
                    return [&, kind] {
                        if (!e->is(kind)) {
                            fmterr(tknz::token_kind(kind), e);
                            return;
                        }
                        result.token = e->to_string();
                        result.kind = keyword_cache;
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
                    result.kind = KeyWord::eof;
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
                                 ret = MatchState::not_match;
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
                                 ret = MatchState::not_match;
                                 return;
                             }
                             if (!e->has("\"")) {
                                 fmterr("string", e);
                                 return;
                             }
                             r.consume();
                             result.kind = KeyWord::string;
                             ret = MatchState::succeed;
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
                             report("indent is not implemented");
                             ret = MatchState::fatal;
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
                        tok += e->to_string();
                        r.consume();
                    }
                    if (tok.size() == 0) {
                        fmterr("not-space", e);
                        return MatchState::not_match;
                    }
                    result.token = std::move(tok);
                    result.kind = KeyWord::not_space;
                    return MatchState::succeed;
                }
                else if (is_keyword(KeyWord::until_eol)) {
                    String tok;
                    while (true) {
                        e = r.get();
                        if (!e || e->is(tknz::TokenKind::line)) {
                            break;
                        }
                        tok += e->to_string();
                        r.consume();
                    }
                    if (tok.size() == 0) {
                        fmterr("not-eol", e);
                        return MatchState::not_match;
                    }
                    result.token = std::move(tok);
                    result.kind = KeyWord::until_eol;
                    return MatchState::succeed;
                }
                else {
                    report("unsupported keyword `", value->tok->to_string(), "`");
                    return MatchState::fatal;
                }
            }

            template <class String, template <class...> class Vec>
            MatchState match_literal(ErrorContext<String, Vec>& ctx, wrap::shared_ptr<Element<String, Vec>>& v, MatchResult<String>& result) {
                auto report = [&](auto&&... args) {
                    ctx.err.packln("error: ", args...);
                    ctx.errat = ctx.r.get();
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
                    report("expect keyword or symbol but token is ", e->what(), " (symbol `", e->to_string(), "`)");
                    return MatchState::not_match;
                }
                auto str = value->tok->to_string();
                if (!e->has(str)) {
                    report("expect ", str, " but token is ", e->what(), " (symbol `", e->to_string(), "`)");
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
