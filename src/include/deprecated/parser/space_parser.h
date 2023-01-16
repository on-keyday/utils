/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#pragma once

#include "parser_base.h"
#include <helper/space.h>
#include <utf/convert.h>

namespace utils {
    namespace parser {
        template <class Input, class String, class Kind, template <class...> class Vec>
        struct LineParser : Parser<Input, String, Kind, Vec> {
            Kind kind;
            ParseResult<String, Kind, Vec> parse(Sequencer<Input>& seq, Pos& pos) override {
                if (seq.consume_if('\n')) {
                    auto ret = make_token<String, Kind, Vec>(utf::convert<String>("\n"), kind, pos);
                    pos.line++;
                    pos.pos = 0;
                    pos.rptr = seq.rptr;
                    return {ret};
                }
                if (seq.seek_if("\r\n")) {
                    auto ret = make_token<String, Kind, Vec>(utf::convert<String>("\r\n"), kind, pos);
                    pos.line++;
                    pos.pos = 0;
                    pos.rptr = seq.rptr;
                    return {ret};
                }
                if (seq.consume_if('\r')) {
                    auto ret = make_token<String, Kind, Vec>(utf::convert<String>("\r"), kind, pos);
                    pos.line++;
                    pos.pos = 0;
                    pos.rptr = seq.rptr;
                    return {ret};
                }
                return {.err = RawMsgError<String, const char*>{"expect line but not"}};
            }
        };

        template <class Input, class String, class Kind, template <class...> class Vec>
        struct SpaceParser : Parser<Input, String, Kind, Vec> {
            Kind kind;
            ParseResult<String, Kind, Vec> parse(Sequencer<Input>& seq, Pos& pos) override {
                auto be = seq.rptr;
                auto e = space::match_space<true>(seq);
                if (e) {
                    char16_t v[] = {e, 0};
                    auto ret = make_token<String, Kind, Vec>(utf::convert<String>(v), kind, pos);
                    pos.pos += seq.rptr - be;
                    pos.rptr = seq.rptr;
                    return {ret};
                }
                return {.err = RawMsgError<String, const char*>{"expect space but not"}};
            }
        };

        template <class Input, class String, class Kind, template <class...> class Vec>
        struct SpacesParser : Parser<Input, String, Kind, Vec> {
            Kind kind;
            ParseResult<String, Kind, Vec> parse(Sequencer<Input>& seq, Pos& pos) override {
                auto be = seq.rptr;
                auto e = space::match_space<true>(seq);
                if (e) {
                    char16_t v[] = {e, 0};
                    auto e = utf::convert<utf::Minibuffer<typename Sequencer<Input>::char_type>>(v);
                    auto ch = utf::convert<String>(e);
                    auto ret = make_token<String, Kind, Vec>(ch, kind, pos);
                    while (seq.seek_if(e.buf)) {
                        helper::append(ret->token, e.buf);
                    }
                    pos.pos += seq.rptr - be;
                    pos.rptr = seq.rptr;
                    return {ret};
                }
                return {.err = RawMsgError<String, const char*>{"expect space but not"}};
            }
        };

        template <class Input, class String, class Kind, template <class...> class Vec>
        wrap::shared_ptr<LineParser<Input, String, Kind, Vec>> make_line(Kind kind) {
            auto ret = wrap::make_shared<LineParser<Input, String, Kind, Vec>>();
            ret->kind = kind;
            return ret;
        }

        template <class Input, class String, class Kind, template <class...> class Vec>
        wrap::shared_ptr<SpaceParser<Input, String, Kind, Vec>> make_space(Kind kind) {
            auto ret = wrap::make_shared<SpaceParser<Input, String, Kind, Vec>>();
            ret->kind = kind;
            return ret;
        }

        template <class Input, class String, class Kind, template <class...> class Vec>
        wrap::shared_ptr<SpacesParser<Input, String, Kind, Vec>> make_spaces(Kind kind) {
            auto ret = wrap::make_shared<SpacesParser<Input, String, Kind, Vec>>();
            ret->kind = kind;
            return ret;
        }
    }  // namespace parser
}  // namespace utils
