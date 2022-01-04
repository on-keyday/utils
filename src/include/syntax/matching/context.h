/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// context - matching context
#pragma once
#include "../make_parser/element.h"

#include "../../wrap/pack.h"

#include "../make_parser/keyword.h"

namespace utils {
    namespace syntax {

        template <class String, template <class...> class Vec>
        struct ErrorContext {
            Reader<String> r;
            wrap::internal::Pack err;
            wrap::shared_ptr<tknz::Token<String>> errat;
            wrap::shared_ptr<Element<String, Vec>> errelement;
        };

        template <class String>
        struct MatchResult {
            String token;
            KeyWord kind = KeyWord::id;
        };
    }  // namespace syntax
}  // namespace utils
