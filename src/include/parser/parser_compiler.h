/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// parser_compiler - compile parser from text
#pragma once
#include "parser_base.h"
#include "logical_parser.h"
#include "space_parser.h"
#include "token_parser.h"
#include "../wrap/lite/map.h"

namespace utils {
    namespace parser {
        template <class Input, class String, class Kind, template <class...> class Vec, class Src>
        wrap::shared_ptr<Parser<Input, String, Kind, Vec>> compile_parser(Sequencer<Src>& seq) {
            using parser_t = wrap::shared_ptr<Parser<Input, String, Kind, Vec>>;
            wrap::map<String, parser_t> desc;
            helper::space::consume_space(seq, true);
            String tok;
            helper::read_whilef(tok, seq, [&](auto&& c) {
                return c != ':' && !helper::space::match_space(seq);
            });
            helper::space::consume_space(seq, true);
            if (!seq.seek_if(":=")) {
                return nullptr;
            }
            return nullptr;
        }
    }  // namespace parser
}  // namespace utils