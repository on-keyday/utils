/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// complex_parser - complex syntax parser
#pragma once
#include "token_parser.h"
#include "logical_parser.h"

namespace utils {
    namespace parser {
        template <class Input, class String, class Kind, template <class...> class Vec>
        auto string_parser(Kind detkind, Kind symkind, Kind segkind, auto&& segname, auto&& quote, auto&& esc, bool allow_line = false) {
            using parser_t = wrap::shared_ptr<Parser<Input, String, Kind, Vec>>;
            auto strdetail = make_func<Input, String, Kind, Vec>(utils::parser::string_rule(quote, esc, allow_line), detkind);
            auto quote_ = make_tokparser<Input, String, Kind, Vec>(quote, symkind);
            auto str = make_and<Input, String, Kind, Vec>(Vec<parser_t>{quote_, strdetail, quote_}, segname, segkind);
            return str;
        }

        template <class Input, class String, class Kind, template <class...> class Vec>
        auto blank_parser(Kind spacekind, Kind linekind, Kind segkind, auto&& segname, bool no_line = false, bool difkindspace = false) {
            using parser_t = wrap::shared_ptr<Parser<Input, String, Kind, Vec>>;
            auto space = make_spaces<Input, String, Kind, Vec>(spacekind);
            if (no_line && !difkindspace) {
                return parser_t(space);
            }
            auto res = parser_t(space);
            if (!no_line) {
                auto line = make_line<Input, String, Kind, Vec>(linekind);
                auto or_ = make_or<Input, String, Kind, Vec>(Vec<parser_t>{line, space});
                res = or_;
            }
            auto repeat = make_repeat(res, segname, segkind);
            return parser_t(repeat);
        }
    }  // namespace parser
}  // namespace utils
