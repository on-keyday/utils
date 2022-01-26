/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// parser_base - parser base class
#pragma once

#include "../core/sequencer.h"
#include "token.h"

namespace utils {
    namespace parser {
        template <class String, class Kind, template <class...> class Vec>
        struct ParseResult {
            using token_t = Token<String, Kind, Vec>;
            wrap::shared_ptr<token_t> tok;
            bool fatal;
        };

        template <class Input, class String, class Kind, template <class...> class Vec>
        struct Parser {
            using token_t = Token<String, Kind, Vec>;
            using result_t = ParseResult<String, Kind, Vec>;

            virtual result_t parse(Sequencer<Input>& input, Pos& posctx) {
                return {};
            }

            virtual bool none_is_not_error() const {
                return false;
            }
        };

    }  // namespace parser
}  // namespace utils
