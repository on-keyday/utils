/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// regex_parser - parser worked by std::regex
#pragma once
#include <regex>
#include "parser_base.h"
#include "../wrap/lite/string.h"

namespace utils {
    namespace parser {
        template <class Input, class String, class Kind, template <class...> class Vec>
        struct RegexParser {
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
                                return {.err = RawMsgError<String, const char*>{"expect binary regex but input size is not 1"}};
                            }
                            else {
                                in.push_back(c);
                            }
                        }
                        else {
                            if (!utf::convert_one(seq, in)) {
                                seq.rptr = beg;
                                return {.err = RawMsgError<String, const char*>{"unexpected utf code point"}};
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
                    pos = postmp;
                    return nullptr;
                }
                return make_token<String, Kind, Vec>(cpy, kind, pos);
            }

            ParserKind declkind() const override {
                return ParserKind::regex;
            }
        };
    }  // namespace parser
}  // namespace utils
