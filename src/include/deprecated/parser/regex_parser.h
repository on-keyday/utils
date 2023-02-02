/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// regex_parser - parser worked by std::regex
#pragma once
#include <regex>
#include "parser_base.h"
#include <wrap/light/string.h>
#include "token_parser.h"
#include <unicode/utf/convert.h>

namespace utils {
    namespace parser {

        template <class String>
        struct RegexNotMatch {
            String regstr;
            Pos pos_;
            String Error() {
                String msg;
                helper::appends(msg, "regex `", regstr, "` not matched");
                return msg;
            }

            Pos pos() {
                return pos_;
            }
        };

        template <class Input, class String, class Kind, template <class...> class Vec>
        struct RegexParser : Parser<Input, String, Kind, Vec> {
            String regstr;
            std::regex reg;
            size_t least_read = 0;
            bool is_binary = false;
            Kind kind;

            ParseResult<String, Kind, Vec> parse(Sequencer<Input>& seq, Pos& pos) override {
                wrap::string in, cpy;
                size_t beg = seq.rptr;
                size_t count = 0;
                Pos postmp = pos;
                bool lined = false;
                auto incrpos = [&] {
                    pos.pos++;
                    pos.rptr = seq.rptr;
                    if (lined) {
                        pos.line++;
                        pos.pos = 0;
                    }
                };
                while (!seq.eos()) {
                    cpy = in;
                    auto c = seq.current();
                    size_t n = seq.rptr;
                    lined = false;
                    if (number::is_in_ascii_range(c)) {
                        in.push_back(c);
                        seq.consume();
                        if (c == '\r' && seq.current() == '\n') {
                            in.push_back('\n');
                            seq.consume();
                            lined = true;
                        }
                        else if (c == '\r' || c == '\n') {
                            lined = true;
                        }
                    }
                    else {
                        if (is_binary) {
                            if constexpr (sizeof(c) != 1) {
                                seq.rptr = beg;
                                auto b = pos;
                                pos = postmp;
                                return {.err = RawMsgError<String, const char*>{"expect binary regex but input size is not 1", b}};
                            }
                            else {
                                in.push_back(c);
                            }
                        }
                        else {
                            if (!utf::convert_one(seq, in)) {
                                seq.rptr = beg;
                                auto b = pos;
                                pos = postmp;
                                return {.err = RawMsgError<String, const char*>{"unexpected utf code point", b}};
                            }
                        }
                    }
                    count++;
                    if (count <= least_read) {
                        incrpos();
                        continue;
                    }
                    if (!std::regex_match(in, reg)) {
                        seq.rptr = n;
                        break;
                    }
                    incrpos();
                }
                if (cpy.size() == 0 || count <= least_read) {
                    auto b = pos;
                    pos = postmp;
                    return {.err = RegexNotMatch<String>{regstr, b}};
                }
                return {make_token<String, Kind, Vec>(cpy, kind, pos)};
            }

            ParserKind declkind() const override {
                return ParserKind::regex;
            }
        };

        template <class Input, class String, class Kind, template <class...> class Vec>
        struct AnyOtherRegexParser : AnyOtherParser<Input, String, Kind, Vec> {
            String regstr;
            std::regex reg;

            ParseResult<String, Kind, Vec> parse(Sequencer<Input>& seq, Pos& pos) override {
                auto postmp = pos;
                auto ps = AnyOtherParser<Input, String, Kind, Vec>::parse(seq, pos);
                if (!ps.tok) {
                    return ps;
                }
                auto call_match = [&](auto& v) {
                    if (!std::regex_match(v, reg)) {
                        auto b = pos;
                        pos = postmp;
                        return decltype(ps){.err = RegexNotMatch<String>{regstr, b}};
                    }
                    return std::move(ps);
                };
                if constexpr (std::is_same_v<String, wrap::string>) {
                    return call_match(ps.tok->token);
                }
                else {
                    auto tmp = utf::convert<wrap::string>(ps.tok->token);
                    return call_match(tmp);
                }
            }

            ParserKind declkind() const override {
                return ParserKind::reg_other;
            }
        };

        template <class Input, class String, class Kind, template <class...> class Vec, class Reg>
        wrap::shared_ptr<RegexParser<Input, String, Kind, Vec>> make_regex(auto& regstr, Reg&& reg, Kind kind) {
            auto p = wrap::make_shared<RegexParser<Input, String, Kind, Vec>>();
            p->regstr = utf::convert<String>(regstr);
            p->reg = std::move(reg);
            p->kind = kind;
            return p;
        }

        template <class Input, class String, class Kind, template <class...> class Vec>
        wrap::shared_ptr<AnyOtherRegexParser<Input, String, Kind, Vec>> make_regother(auto&& regstr, auto&& reg, Vec<String> keyword, Vec<String> symbol, Kind kind, bool no_space = true, bool no_line = true) {
            auto ret = wrap::make_shared<AnyOtherRegexParser<Input, String, Kind, Vec>>();
            ret->kind = kind;
            ret->keyword = std::move(keyword);
            ret->symbol = std::move(symbol);
            ret->no_space = no_space;
            ret->no_line = no_line;
            ret->regstr = utf::convert<String>(regstr);
            ret->reg = std::move(reg);
            return ret;
        }
    }  // namespace parser
}  // namespace utils
