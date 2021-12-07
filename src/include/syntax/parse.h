/*license*/

// parse - parse method
#pragma once

#include "element.h"
#include "../wrap/pack.h"

namespace utils {
    namespace syntax {

        template <class String>
        struct ParseCtx {
            Reader<String> r;
            wrap::internal::Pack err;
        };

        template <class String, template <class...> class Vec>
        bool parse_attribute(Reader<String>& r, wrap::shared_ptr<Element<String, Vec>>& elm) {
            if (!elm) {
                return false;
            }
            while (true) {
                auto e = r.read();
                if (!e) {
                    break;
                }
                if (e->has(attribute(Attribute::adjacent))) {
                    if (any(elm->attr & Attribute::adjacent)) {
                    }
                }
            }
            return true;
        }

        template <class String, template <class...> class Vec>
        bool parse_single(Reader<String>& r, wrap::shared_ptr<Element<String, Vec>>& single) {
            auto e = r.read();
            if (!e) {
                return false;
            }
            if (e->has("\"")) {
                e = r.consume_get();
                if (!e->is(tknz::TokenKind::comment)) {
                    return false;
                }
                auto s = wrap::make_shared<Single<String, Vec>>();
                s->type = SyntaxType::literal;
                s->tok = e;
                single = s;
                e = r.consume_get();
                if (!e) {
                    return false;
                }
                if (!e->has("\"")) {
                    return false;
                }
            }
            else if (e->is(tknz::TokenKind::keyword)) {
                auto s = wrap::make_shared<Single<String, Vec>>();
                s->type = SyntaxType::keyword;
                s->tok = e;
                single = s;
                r.consume();
            }
            else if (e->is(tknz::TokenKind::identifier)) {
                auto s = wrap::make_shared<Single<String, Vec>>();
                s->type = SyntaxType::reference;
                s->tok = e;
                single = s;
                r.consume();
            }
            else {
                return false;
            }
        }

        template <class String, template <class...> class Vec>
        bool parse_group(Reader<String>& r, wrap::shared_ptr<Element<String, Vec>>& group) {
            auto gr = wrap::make_shared<Group<String, Vec>>();
            gr->type = SyntaxType::group;
            auto e = r.read();
            if (!e) {
                return false;
            }
        }
    }  // namespace syntax
}  // namespace utils