/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// token_parser - token parser
#pragma once

#include "parser_base.h"

namespace utils {
    namespace parser {
        template <class Input, class String, class Kind, template <class...> class Vec>
        struct TokenParser : Parser<Input, String, Kind, Vec> {
            Kind kind;
            String token;
            ParseResult<String, Kind, Vec> parse(Sequencer<Input>& seq, Pos& pos) override {
                size_t beg = seq.rptr;
                if (seq.seek_if(token)) {
                    auto ret = make_token<String, Kind, Vec>(token, kind, pos);
                    pos.pos += seq.rptr - beg;
                    return ret;
                }
                return {};
            }
        };

        template <class Input, class String, class Kind, template <class...> class Vec, class String2>
        wrap::shared_ptr<TokenParser<Input, String, Kind, Vec>> make_repeat(wrap::shared_ptr<Parser<Input, String, Kind, Vec>> sub, String2 token, Kind kind) {
            auto ret = wrap::make_shared<TokenParser<Input, String, Kind, Vec>>();
            ret->token = std::move(token);
            ret->kind = kind;
            return ret;
        }
    }  // namespace parser
}  // namespace utils
