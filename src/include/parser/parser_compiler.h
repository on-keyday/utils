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

        namespace internal {
            template <class Input, class String, class Kind, template <class...> class Vec, class Src>
            wrap::shared_ptr<Parser<Input, String, Kind, Vec>> read_str(Sequencer<Src>& seq) {
                if (seq.seek_if("\"")) {
                    auto reader = helper::string_reader("\"", "\\");
                }
            }
        }  // namespace internal

        template <class Input, class String, class Kind, template <class...> class Vec, class Src>
        wrap::shared_ptr<Parser<Input, String, Kind, Vec>> compile_parser(Sequencer<Src>& seq) {
            using parser_t = wrap::shared_ptr<Parser<Input, String, Kind, Vec>>;
#define CONSUME_SPACE() helper::space::consume_space(seq, true);
            wrap::map<String, parser_t> desc;
            CONSUME_SPACE()
            String tok;
            helper::read_whilef(tok, seq, [&](auto&& c) {
                return c != ':' && !helper::space::match_space(seq, true);
            });
            CONSUME_SPACE()
            if (!seq.seek_if(":=")) {
                return nullptr;
            }
            CONSUME_SPACE()

            return nullptr;
        }
    }  // namespace parser
}  // namespace utils
