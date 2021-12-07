/*license*/

// parse - parse method
#pragma once

#include "element.h"
#include "../wrap/pack.h"

namespace utils {
    namespace syntax {

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
            auto checK_attr = [&](auto name, auto kind) {
                if (e->has(attribute(kind))) {
                    if (any(elm->attr & kind)) {
                        err(name, kind);
                        return -1;
                    }
                    elm->attr |= kind;
                    return 1;
                }
                return 0;
            };
            while (true) {
                auto e = r.read();
                if (!e) {
                    break;
                }
                if (auto res = checK_attr("adjacent", Attribute::adjacent)) {
                    if (res < 0) {
                        return false;
                    }
                    continue;
                }
                else if (res = checK_attr("fatal", Attribute::fatal)) {
                    if (res < 0) {
                        return false;
                    }
                    continue;
                }
                else if (res = checK_attr("ifexists", Attribute::ifexists)) {
                    if (res < 0) {
                        return false;
                    }
                    continue;
                }
                else if (res = checK_attr("repeat", Attribute::repeat)) {
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
            auto& r = ctx.r;
            auto e = r.read();
            if (!e || e->is(tknz::TokenKind::line)) {
                return true;
            }
        }

        template <class String, template <class...> class Vec>
        bool parse_group(ParseContext<String>& ctx, wrap::shared_ptr<Element<String, Vec>>& group) {
            auto gr = wrap::make_shared<Group<String, Vec>>();
            gr->type = SyntaxType::group;
            auto& r = ctx.r;
            while (true) {
                auto e = r.read();
                if (!e || e->is(tknz::TokenKind::line) || e->has("|")) {
                    break;
                }
                wrap::shared_ptr<Element<String, Vec>> elm;
                if (e->has("[")) {
                    r.consume();
                    if (!parse_or(ctx, elm, true)) {
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
                ctx.err.pack("error: expect syntax elment but not");
                return false;
            }
            group = gr;
            return true;
        }
    }  // namespace syntax
}  // namespace utils
