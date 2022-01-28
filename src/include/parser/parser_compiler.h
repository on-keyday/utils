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
#include "../escape/read_string.h"

namespace utils {
    namespace parser {

        enum class KindMap {
            error,
            token,
            anyother,
            and_,
            repeat,
            fatal,
        };
#define CONSUME_SPACE(line)                  \
    helper::space::consume_space(seq, line); \
    if (seq.eos()) return nullptr;
        namespace internal {
            template <class Input, class String, class Kind, template <class...> class Vec, class Src, class Fn>
            wrap::shared_ptr<Parser<Input, String, Kind, Vec>> wrap_elm(wrap::shared_ptr<Parser<Input, String, Kind, Vec>> w, Sequencer<Src>& seq, Fn&& fn) {
                bool repeat = false, allow_none = false, fatal = false;
                using parser_t = wrap::shared_ptr<Parser<Input, String, Kind, Vec>>;
                while (true) {
                    CONSUME_SPACE(false)
                    if (!repeat && seq.consume_if('*')) {
                        repeat = true;
                        continue;
                    }
                    else if (!allow_none && seq.consume_if('?')) {
                        allow_none = true;
                        continue;
                    }
                    else if (!fatal && seq.consume_if('!')) {
                        fatal = true;
                        continue;
                    }
                    break;
                }
                auto v = w;
                if (fatal) {
                    auto kd = fn("(fatal)", KindMap::fatal);
                    auto tmp = make_func<Input, String, Kind, Vec>(
                        [w](auto& seq, auto& tok, auto& flag, auto& pos) {
                            auto b = pos;
                            auto tmp = w->parse(seq, pos);
                            if (!tmp.tok || tmp.fatal) {
                                flag = -1;
                                return false;
                            }
                            flag = 2;
                            tok = tmp.tok;
                            return true;
                        },
                        kd);
                    v = parser_t(tmp);
                }
                if (repeat) {
                    auto kd = fn("(repeat)", KindMap::repeat);
                    auto tmp = make_repeat<Input, String, Kind, Vec>(v, "(repeat)", kd);
                    v = parser_t(tmp);
                }
                return v;
            }

            template <class Input, class String, class Kind, template <class...> class Vec, class Src, class Fn>
            wrap::shared_ptr<TokenParser<Input, String, Kind, Vec>> read_str(Sequencer<Src>& seq, Fn&& fn) {
                auto beg = seq.rptr;
                String str;
                if (!escape::read_string(str, seq, escape::ReadFlag::escape, escape::go_prefix())) {
                    seq.rptr = beg;
                    return nullptr;
                }
                auto kd = fn(str, KindMap::token);
                return make_tokparser<Input, String, Kind, Vec>(std::move(str), kd);
            }

            template <class Input, class String, class Kind, template <class...> class Vec, class Src, class Fn>
            wrap::shared_ptr<TokenParser<Input, String, Kind, Vec>> read_anyother(Sequencer<Src>& seq, Fn&& fn, auto& mp) {
                auto beg = seq.rptr;
                String tok;
                if (!helper::read_whilef(tok, seq, [&](auto&& c) {
                        return c != ':' && c != '|' && c != '[' &&
                               c != ']' && c != '*' && c != '\"' && c != '\\' &&
                               c != '&' && c != '\'' && c != '`' && c != '=' &&
                               c != '!' && c != '?' && c != '(' && c != ')' && c != ',' &&
                               !helper::space::match_space(seq, true);
                    })) {
                    seq.rptr = beg;
                    return nullptr;
                }
                auto kd = fn(tok, KindMap::anyother);
                return make_tokparser<Input, String, Kind, Vec>(std::move(tok), kd);
            }

            template <class Kind>
            constexpr auto def_Fn() {
                return [](auto&&, auto&&) { return Kind{}; };
            }
        }  // namespace internal

        template <class Input, class String, class Kind, template <class...> class Vec, class Src, class Fn = decltype(internal::def_Fn<Kind>())>
        wrap::shared_ptr<Parser<Input, String, Kind, Vec>> compile_parser(
            Sequencer<Src>& seq, Fn&& fn = internal::def_Fn<Kind>()) {
            using parser_t = wrap::shared_ptr<Parser<Input, String, Kind, Vec>>;
            using and_t = wrap::shared_ptr<AndParser<Input, String, Kind, Vec>>;
            using or_t = wrap::shared_ptr<OrParser<Input, String, Kind, Vec>>;

            wrap::map<String, parser_t> desc;
            CONSUME_SPACE(true)
            String tok;
            if (!helper::read_whilef<true>(tok, seq, [&](auto&& c) {
                    return c != ':' && !helper::space::match_space(seq, true);
                })) {
                return nullptr;
            }
            CONSUME_SPACE(true)
            if (!seq.seek_if(":=")) {
                return nullptr;
            }
            CONSUME_SPACE(true)
            parser_t res;
            and_t and_;
            or_t or_;
            if (auto p = internal::read_str<Input, String, Kind, Vec>(seq, fn)) {
                auto w = internal::wrap_elm<Input, String, Kind, Vec>(p, seq, fn);
                if (!res) {
                    res = w;
                }
                else {
                    if (!and_) {
                        auto kd = fn(tok, KindMap::and_);
                        and_ = make_and<Input, String, Kind, Vec>(Vec<parser_t>{res}, tok, kd);
                        res = and_;
                    }
                    and_->add_parser(w);
                }
            }
            return res;
        }
#undef CONSUME_SPACE
    }  // namespace parser
}  // namespace utils
