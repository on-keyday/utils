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
        };

        namespace internal {
            template <class Input, class String, class Kind, template <class...> class Vec, class Src, class Fn>
            wrap::shared_ptr<Parser<Input, String, Kind, Vec>> wrap_elm(auto& w, Sequencer<Src>& seq, Fn&& fn) {
                bool repeat = false, allow_none = false, fatal = false;
            }

            template <class Input, class String, class Kind, template <class...> class Vec, class Src, class Fn>
            wrap::shared_ptr<Parser<Input, String, Kind, Vec>> read_str(Sequencer<Src>& seq, Fn&& fn) {
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
            wrap::shared_ptr<Parser<Input, String, Kind, Vec>> read_anyother(Sequencer<Src>& seq, Fn&& fn, auto& mp) {
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
#define CONSUME_SPACE() helper::space::consume_space(seq, true);
            wrap::map<String, parser_t> desc;
            CONSUME_SPACE()
            String tok;
            if (!helper::read_whilef<true>(tok, seq, [&](auto&& c) {
                    return c != ':' && !helper::space::match_space(seq, true);
                })) {
                return nullptr;
            }
            CONSUME_SPACE()
            if (!seq.seek_if(":=")) {
                return nullptr;
            }

            CONSUME_SPACE()
            parser_t res;
            and_t and_;
            or_t or_;
            if (auto p = internal::read_str<Input, String, Kind, Vec>(seq, fn)) {
                if (!res) {
                    res = p;
                }
                else {
                    if (!and_) {
                        auto kd = fn(tok, KindMap::and_);
                        and_ = make_and<Input, String, Kind, Vec>(Vec<parser_t>{res}, tok, kd);
                        res = and_;
                    }
                    and_->add_parser(p);
                }
            }
            return res;
        }
    }  // namespace parser
}  // namespace utils
