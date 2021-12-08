/*license*/

// parse_impl - parse method implementation
#pragma once

#include "element.h"
#include "../../wrap/pack.h"

namespace utils {
    namespace syntax {
        namespace internal {
            template <class String>
            struct ParseContext {
                Reader<String> r;
                wrap::internal::Pack err;
            };

            template <class String, template <class...> class Vec>
            bool parse_attribute(ParseContext<String>& ctx, wrap::shared_ptr<Element<String, Vec>>& elm) {
                auto& r = ctx.r;
                if (!elm) {
                    return false;
                }
                auto err = [&](auto& name, auto e) {
                    ctx.err.packln("error: attribute `", name, "` (symbol `", attribute(e), "`) is already exists.");
                };
                // when below here `auto e = r.read()` is not exist, clang crash at parsing lambda
                auto e = r.read();
                auto checK_attr = [&](auto name, auto kind) {
                    // when `auto e=r.read()` is not exist
                    // below `e->has(attribute(kind))` must become compile error
                    // because `e` is not defined,
                    // but clang is crash.
                    if (e->has(attribute(kind))) {
                        if (any(elm->attr & kind)) {
                            err(name, kind);
                            return -1;
                        }
                        elm->attr |= kind;
                        r.consume();
                        return 1;
                    }
                    return 0;
                };
                while (true) {
                    e = r.read();
                    if (!e) {
                        break;
                    }
                    if (auto res = checK_attr("adjacent", Attribute::adjacent); res) {
                        if (res < 0) {
                            return false;
                        }
                        continue;
                    }
                    else if (res = checK_attr("fatal", Attribute::fatal); res) {
                        if (res < 0) {
                            return false;
                        }
                        continue;
                    }
                    else if (res = checK_attr("ifexists", Attribute::ifexists); res) {
                        if (res < 0) {
                            return false;
                        }
                        continue;
                    }
                    else if (res = checK_attr("repeat", Attribute::repeat); res) {
                        if (res < 0) {
                            return false;
                        }
                        continue;
                    }
                    break;
                }
                return true;
            }

            template <class String, template <class...> class Vec>
            bool parse_single(ParseContext<String>& ctx, wrap::shared_ptr<Element<String, Vec>>& single) {
                auto& r = ctx.r;
                auto e = r.read();
                if (!e) {
                    return false;
                }
                wrap::shared_ptr<Single<String, Vec>> s;
                if (e->has("\"")) {
                    e = r.consume_get();
                    if (!e->is(tknz::TokenKind::comment)) {
                        return false;
                    }
                    s = wrap::make_shared<Single<String, Vec>>();
                    s->type = SyntaxType::literal;
                    s->tok = e;
                    single = s;
                    e = r.consume_get();
                    if (!e) {
                        ctx.err.packln("unexpected EOF. expect \"");
                        return false;
                    }
                    if (!e->has("\"")) {
                        ctx.err.packln("expect \" but token is ", e->to_string());
                        return false;
                    }
                    r.consume();
                }
                else if (e->is(tknz::TokenKind::keyword)) {
                    s = wrap::make_shared<Single<String, Vec>>();
                    s->type = SyntaxType::keyword;
                    s->tok = e;
                    single = s;
                    r.consume();
                }
                else if (e->is(tknz::TokenKind::identifier)) {
                    s = wrap::make_shared<Single<String, Vec>>();
                    s->type = SyntaxType::reference;
                    s->tok = e;
                    single = s;
                    r.consume();
                }
                else {
                    ctx.err.pack("error: expect literal, keyword, or identifier  but token is ", e->to_string());
                    return false;
                }
                if (!parse_attribute(ctx, single)) {
                    ctx.err.pack("note: at syntax element ");
                    if (s->type == SyntaxType::literal) {
                        ctx.err.packln("\"", s->tok->to_string(), "\"");
                    }
                    else {
                        ctx.err.packln(s->tok->to_string());
                    }
                    return false;
                }
                return true;
            }

            template <class String, template <class...> class Vec>
            bool parse_group(ParseContext<String>& ctx, wrap::shared_ptr<Element<String, Vec>>& group);

            template <class String, template <class...> class Vec>
            bool parse_or(ParseContext<String>& ctx, wrap::shared_ptr<Element<String, Vec>>& group, bool endbrace) {
                if (!parse_group(ctx, group)) {
                    return false;
                }
                wrap::shared_ptr<Or<String, Vec>> or_;
                auto& r = ctx.r;
                while (true) {
                    auto e = r.read();
                    if (!e || e->is(tknz::TokenKind::line)) {
                        if (endbrace) {
                            ctx.err.packln("error: expect ] but token is EOL");
                            return false;
                        }
                        if (or_) {
                            group = or_;
                        }
                        return true;
                    }
                    if (endbrace && e->has("]")) {
                        r.consume();
                        if (or_) {
                            group = or_;
                        }
                        return true;
                    }
                    if (!e->has("|")) {
                        ctx.err.packln("error: expect | but token is ", e->to_string());
                        return false;
                    }
                    r.consume();
                    if (!or_) {
                        or_ = wrap::make_shared<Or<String, Vec>>();
                        or_->type = SyntaxType::or_;
                        or_->or_list.push_back(std::move(group));
                    }
                    if (!parse_group(ctx, group)) {
                        return false;
                    }
                    or_->or_list.push_back(std::move(group));
                }
                return true;
            }

            template <class String, template <class...> class Vec>
            bool parse_group(ParseContext<String>& ctx, wrap::shared_ptr<Element<String, Vec>>& group) {
                auto gr = wrap::make_shared<Group<String, Vec>>();
                gr->type = SyntaxType::group;
                auto& r = ctx.r;
                while (true) {
                    auto e = r.read();
                    if (!e || e->is(tknz::TokenKind::line) || e->has("|") || e->has("]")) {
                        break;
                    }
                    wrap::shared_ptr<Element<String, Vec>> elm;
                    if (e->has("[")) {
                        r.consume();
                        if (!parse_or(ctx, elm, true)) {
                            return false;
                        }
                        if (!parse_attribute(ctx, elm)) {
                            ctx.err.packln("note: at parsing []");
                            return false;
                        }
                    }
                    else {
                        if (!parse_single(ctx, elm)) {
                            return false;
                        }
                    }
                    gr->group.push_back(std::move(elm));
                }
                if (!gr->group.size()) {
                    ctx.err.packln("error: expect syntax elment but not");
                    return false;
                }
                group = gr;
                return true;
            }

            template <class String, template <class...> class Vec>
            bool parse_a_line(ParseContext<String>& ctx, String& segname, wrap::shared_ptr<Element<String, Vec>>& group) {
                auto& r = ctx.r;
                r.ignore_line = true;
                auto e = r.read();
                if (!e) {
                    return false;
                }
                if (!e->is(tknz::TokenKind::identifier)) {
                    ctx.err.packln("error: expect identifier but token is `", e->what(), "` (symbol `", e->to_string(), "`)");
                    return false;
                }
                segname = e->to_string();
                e = r.consume_read();
                if (!e) {
                    ctx.err.packln("error: expect `:=` but token is EOF");
                    return false;
                }
                if (!e->has(":=")) {
                    ctx.err.packln("error: expect `:=` but token is `", e->what(), "` (symbol `", e->to_string(), "`)");
                    return false;
                }
                r.consume_read();
                r.ignore_line = false;
                return parse_or(ctx, group, false);
            }

        }  // namespace internal
    }      // namespace syntax
}  // namespace utils
