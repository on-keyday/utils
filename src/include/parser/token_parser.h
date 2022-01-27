/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// token_parser - token parser
#pragma once

#include "parser_base.h"
#include "../helper/readutil.h"

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
                    return {ret};
                }
                return {};
            }
        };

        template <class Input, class String, class Kind, template <class...> class Vec>
        struct AnyOtherParser : Parser<Input, String, Kind, Vec> {
            Kind kind;
            Vec<String> keyword;
            Vec<String> symbol;
            bool no_space = false;
            bool no_line = false;
            ParseResult<String, Kind, Vec> parse(Sequencer<Input>& seq, Pos& pos) override {
                size_t beg = seq.rptr;
                wrap::shared_ptr<Token<String, Kind, Vec>> ret;
                while (!seq.eos()) {
                    if (no_space) {
                        if (helper::space::match_space<false>(seq)) {
                            break;
                        }
                    }
                    if (no_line) {
                        if (helper::match_eol<false>(seq)) {
                            break;
                        }
                    }
                    bool end = false;
                    for (auto& v : symbol) {
                        if (seq.match(v)) {
                            end = true;
                            break;
                        }
                    }
                    if (end) {
                        break;
                    }
                    if (!ret) {
                        ret = make_token<String, Kind, Vec>(String{}, kind, pos);
                    }
                    ret->token.push_back(seq.current());
                    seq.consume();
                }
                if (!ret) {
                    return {};
                }
                for (auto& k : keyword) {
                    if (helper::equal(k, ret->token)) {
                        seq.rptr = beg;
                        return {};
                    }
                }
                pos.pos += seq.rptr - beg;
                return {ret};
            }
        };

        template <class Input, class String, class Kind, template <class...> class Vec, class String2>
        wrap::shared_ptr<TokenParser<Input, String, Kind, Vec>> make_tokparser(String2 token, Kind kind) {
            auto ret = wrap::make_shared<TokenParser<Input, String, Kind, Vec>>();
            ret->token = std::move(token);
            ret->kind = kind;
            return ret;
        }

        template <class Input, class String, class Kind, template <class...> class Vec>
        wrap::shared_ptr<AnyOtherParser<Input, String, Kind, Vec>> make_anyother(Vec<String> keyword, Vec<String> symbol, Kind kind, bool no_space = true, bool no_line = true) {
            auto ret = wrap::make_shared<AnyOtherParser<Input, String, Kind, Vec>>();
            ret->kind = kind;
            ret->keyword = std::move(keyword);
            ret->symbol = std::move(symbol);
            ret->no_space = no_space;
            ret->no_line = no_line;
            return ret;
        }
    }  // namespace parser
}  // namespace utils
