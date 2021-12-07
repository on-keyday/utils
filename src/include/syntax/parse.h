/*license*/

// parse - parse method
#pragma once

#include "element.h"

namespace utils {
    namespace syntax {

        template <class String, template <class...> class Vec>
        bool parse_single(Reader<String>& r, wrap::shared_ptr<Element<String, Vec>>& single) {
            auto e = r.read();
            if (e->has("\"")) {
                e = r.consume_get();
                if (!e->is(tknz::TokenKind::comment)) {
                    return false;
                }
                auto s = wrap::make_shared<Single<String, Vec>>();
                s->type = SyntaxType::literal;
                s->tok = e;
                e = r.consume_get();
                if (!e) {
                    return false;
                }
                if (!e->has("\"")) {
                    return false;
                }
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