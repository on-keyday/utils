/*license*/

// element - define syntax element type
#pragma once

#include "attribute.h"
#include "reader.h"

namespace utils {
    namespace syntax {
        enum class SyntaxType {
            literal,
            keyword,
            reference,
            group,
            or_,
        };

        template <class String, template <class...> class Vec>
        struct Element {
            SyntaxType type;
            Attribute attr;
        };

        template <class String, template <class...> class Vec>
        struct Single : Element<String, Vec> {
            wrap::shared_ptr<tknz::Token<String>> tok;
        };

        template <class String, template <class...> class Vec>
        struct Group : Element<String, Vec> {
            using elm_t = wrap::shared_ptr<Element<String, Vec>>;
            Vec<elm_t> group;
        };

        template <class String, template <class...> class Vec>
        struct Or : Element<String, Vec> {
            using elm_t = wrap::shared_ptr<Element<String, Vec>>;
            Vec<elm_t> or_list;
        };
    }  // namespace syntax
}  // namespace utils
