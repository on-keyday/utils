/*license*/

// context - matching context
#pragma once
#include "../make_parser/element.h"

#include "../../wrap/pack.h"

namespace utils {
    namespace syntax {

        template <class String, template <class...> class Vec>
        struct Context {
            Reader<String> r;
            wrap::internal::Pack err;
            wrap::shared_ptr<tknz::Token<String>> errat;
            wrap::shared_ptr<Element<String, Vec>> errelement;
        };
    }  // namespace syntax
}  // namespace utils
