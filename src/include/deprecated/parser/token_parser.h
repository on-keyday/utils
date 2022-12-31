/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// token_parser - token parser
#pragma once

#include "parser_base.h"
#include <helper/readutil.h>
#include <helper/strutil.h>
#include <helper/appender.h>
#include <number/parse.h>
#include <number/prefix.h>
#include "ifaces.h"
#include <helper/equal.h>
#include <helper/strutil.h>
#include <view/slice.h>

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
                    pos.rptr = seq.rptr;
                    return {ret};
                }
                return {.err = TokenNotMatchError<String>{token, pos}};
            }

            ParserKind declkind() const override {
                return ParserKind::token;
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
                        if (space::match_space<false>(seq)) {
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
                        return {.err = UnexpectedTokenError<String>{ret->token, pos}};
                    }
                }
                pos.pos += seq.rptr - beg;
                pos.rptr = seq.rptr;
                return {ret};
            }

            ParserKind declkind() const override {
                return ParserKind::anyother;
            }
        };

        template <class Input, class String, class Kind, template <class...> class Vec>
        struct FuncParser : Parser<Input, String, Kind, Vec> {
            template <class T>
            constexpr FuncParser(T&& f)
                : func(std::forward<T>(f)) {}
            Func<Input, String, Kind, Vec> func;
            Kind kind;
            ParseResult<String, Kind, Vec> parse(Sequencer<Input>& seq, Pos& pos) override {
                size_t beg = seq.rptr;
                int flag = 0;
                auto ret = make_token<String, Kind, Vec>(String{}, kind, pos);
                error<String> err;
                if (!func(seq, ret, flag, pos, err)) {
                    seq.rptr = beg;
                    if (flag <= 0) {
                        return {.fatal = flag < 0, .err = std::move(err)};
                    }
                }
                if (flag == 1) {
                    pos.pos += seq.rptr - beg;
                    pos.rptr = seq.rptr;
                }
                return {ret};
            }

            ParserKind declkind() const override {
                return ParserKind::func;
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

        template <class Input, class String, class Kind, template <class...> class Vec, class Func>
        wrap::shared_ptr<FuncParser<Input, String, Kind, Vec>> make_func(Func&& func, Kind kind) {
            auto ret = wrap::make_shared<FuncParser<Input, String, Kind, Vec>>(std::forward<Func>(func));
            ret->kind = kind;
            return ret;
        }

        constexpr auto string_reader(auto&& end, auto&& esc, bool allow_line = false) {
            return [=](auto& seq, auto& tok) {
                while (!seq.eos()) {
                    if (auto n = seq.match_n(end)) {
                        auto sz = bufsize(esc);
                        if (sz) {
                            if (helper::ends_with(tok, esc)) {
                                auto sl = view::make_ref_slice(tok, 0, tok.size() - sz);
                                if (!helper::ends_with(sl, esc)) {
                                    return true;
                                }
                                seq.consume(n);
                                helper ::append(tok, end);
                                continue;
                            }
                        }
                        return true;
                    }
                    auto c = seq.current();
                    if (c == '\n' || c == '\r') {
                        if (!allow_line) {
                            return false;
                        }
                    }
                    tok.push_back(c);
                    seq.consume();
                }
                return false;
            };
        }

        auto string_rule(auto& end, auto& esc, bool allow_line) {
            auto strreader = string_reader(end, esc, allow_line);
            return [strreader](auto& seq, auto& tok, int& flag, auto& pos, auto& err) {
                if (!strreader(seq, tok->token)) {
                    flag = -1;
                    pos.rptr = seq.rptr;
                    err = RawMsgError<decltype(tok->token), const char*>{
                        "unexpected string like",
                        pos,
                    };
                    return false;
                }
                flag = 1;
                return true;
            };
        }

        template <class String>
        struct NumberError {
            number::NumErr err;
            String Error() {
                return utf::convert<String>(error_message(err));
            }
        };

        auto number_rule(bool only_int = false, bool none_prefix = false) {
            return [=](auto& seq, auto& tok, int& flag, auto&, auto& err) {
                size_t beg = seq.rptr;
                int radix = 10;
                if (!none_prefix) {
                    auto pf = number::has_prefix(seq);
                    if (pf) {
                        seq.consume(2);
                        radix = pf;
                    }
                }
                if (!only_int && (radix == 10 || radix == 16)) {
                    if (auto e = number::parse_float(seq, tok->token, radix); !e) {
                        seq.rptr = beg;
                        flag = 0;
                        err = NumberError<decltype(tok->token)>{e};
                        return false;
                    }
                }
                else {
                    if (auto e = number::parse_integer(seq, tok->token, radix); !e) {
                        seq.rptr = beg;
                        flag = 0;
                        err = NumberError<decltype(tok->token)>{e};
                        return false;
                    }
                }
                flag = 1;
                return true;
            };
        }

    }  // namespace parser
}  // namespace utils
