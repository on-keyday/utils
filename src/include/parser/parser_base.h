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
//#include "ifaces.h"

namespace utils {
    namespace parser {
        template <class String, class Kind, template <class...> class Vec>
        struct ParseResult {
            using token_t = Token<String, Kind, Vec>;
            wrap::shared_ptr<token_t> tok;
            bool fatal;
            //error<String> err;
        };

        enum class ParserKind {
            null,
            token,
            space,
            line,
            anyother,
            func,
            or_,
            and_,
            repeat,
            allow_none,
            some_pattern,
            only_one,
            weak_ref,
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

            virtual ParserKind declkind() const {
                return ParserKind::null;
            }
        };

    }  // namespace parser
}  // namespace utils
