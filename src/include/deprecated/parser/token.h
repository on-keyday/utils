/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// token - generic token
#pragma once

#include <wrap/light/smart_ptr.h>

namespace utils {
    namespace parser {
        struct Pos {
            size_t line = 0;
            size_t pos = 0;
            size_t rptr = 0;
        };

        template <class String, class Kind, template <class...> class Vec>
        struct Token {
            String token;
            Kind kind;
            Pos pos;
            Vec<wrap::shared_ptr<Token>> child;
            wrap::weak_ptr<Token> parent;
        };

        template <class String, class Kind, template <class...> class Vec>
        wrap::shared_ptr<Token<String, Kind, Vec>> make_token(String tok, Kind kind, Pos pos,
                                                              wrap::shared_ptr<Token<String, Kind, Vec>> parent = nullptr) {
            auto ret = wrap::make_shared<Token<String, Kind, Vec>>();
            ret->token = std::forward<String>(tok);
            ret->kind = kind;
            ret->pos = std::move(pos);
            if (parent) {
                ret->parent = parent;
                parent->child.push_back(ret);
            }
            return ret;
        }

    }  // namespace parser
}  // namespace utils
