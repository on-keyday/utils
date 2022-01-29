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
            or_,
            repeat,
            fatal,
            allow_none,
        };
#define CONSUME_SPACE(line, eof)             \
    helper::space::consume_space(seq, line); \
    if (eof && seq.eos()) return nullptr;
        namespace internal {
            template <class Input, class String, class Kind, template <class...> class Vec, class Src, class Fn>
            wrap::shared_ptr<Parser<Input, String, Kind, Vec>> wrap_elm(wrap::shared_ptr<Parser<Input, String, Kind, Vec>> w, Sequencer<Src>& seq, Fn&& fn) {
                bool repeat = false, allow_none = false, fatal = false;
                using parser_t = wrap::shared_ptr<Parser<Input, String, Kind, Vec>>;
                while (true) {
                    CONSUME_SPACE(false, false)
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
                    v = tmp;
                }
                if (repeat) {
                    auto kd = fn("(repeat)", KindMap::repeat);
                    auto tmp = make_repeat<Input, String, Kind, Vec>(v, "(repeat)", kd);
                    v = tmp;
                }
                if (allow_none) {
                    auto kd = fn("(allow_none)", KindMap::allow_none);
                    auto tmp = make_allownone<Input, String, Kind, Vec>(v, "(allownone)", kd);
                    v = tmp;
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

            template <class Input, class String, class Kind, template <class...> class Vec>
            struct WeekRef : Parser<Input, String, Kind, Vec> {
                Kind kind;
                String tok;
                ParserKind declkind() override {
                    return ParserKind::week_ref;
                }
            };

            template <class Input, class String, class Kind, template <class...> class Vec, class Src, class Fn>
            wrap::shared_ptr<TokenParser<Input, String, Kind, Vec>> read_anyother(Sequencer<Src>& seq, Fn&& fn) {
                auto beg = seq.rptr;
                String tok;
                if (!helper::read_whilef(tok, seq, [&](auto&& c) {
                        return c != ':' && c != '|' && c != '[' && c != '/' &&
                               c != ']' && c != '*' && c != '\"' && c != '\\' &&
                               c != '&' && c != '\'' && c != '`' && c != '=' &&
                               c != '!' && c != '?' && c != '(' && c != ')' && c != ',' &&
                               !helper::space::match_space(seq, true);
                    })) {
                    seq.rptr = beg;
                    return nullptr;
                }
                auto kd = fn(tok, KindMap::anyother);
                auto v = wrap::make_shared<WeekRef<Input, String, Kind, Vec>>();
                v->token = std::move(tok);
                v->kind = kd;
                return v;
            }

            template <class Kind>
            constexpr auto def_Fn() {
                return [](auto&&, auto&&) { return Kind{}; };
            }

            template <class Input, class String, class Kind, template <class...> class Vec, class Src, class Fn>
            wrap::shared_ptr<Parser<Input, String, Kind, Vec>> compile_seq(auto& tok, Sequencer<Src>& seq, Fn&& fn);

            template <class Input, class String, class Kind, template <class...> class Vec, class Src, class Fn>
            wrap::shared_ptr<Parser<Input, String, Kind, Vec>> compile_primary(auto& tok, Sequencer<Src>& seq, Fn&& fn) {
                auto wrap_code = [&](auto& p) {
                    return wrap_elm<Input, String, Kind, Vec>(p, seq, fn);
                };
                CONSUME_SPACE(false, false)
                if (auto p = read_str<Input, String, Kind, Vec>(seq, fn)) {
                    return wrap_code(p);
                }
                else if (auto tok = read_anyother<Input, String, Kind, Vec>(seq, fn)) {
                    return wrap_code(tok);
                }
                else if (seq.consume_if('[')) {
                    auto tok = compile_seq<Input, String, Kind, Vec>(seq, fn);
                    if (!tok || !seq.consume_if(']')) {
                        return nullptr;
                    }
                    return wrap_code(tok);
                }
                else {
                    return nullptr;
                }
            }

            template <class Input, class String, class Kind, template <class...> class Vec, class Src, class Fn>
            wrap::shared_ptr<Parser<Input, String, Kind, Vec>> compile_seq(auto& tok, Sequencer<Src>& seq, Fn&& fn) {
                auto root = compile_primary(tok, seq, fn);
                if (!root) {
                    return nullptr;
                }
                using and_t = wrap::shared_ptr<AndParser<Input, String, Kind, Vec>>;
                and_t and_;
                while (!seq.eos()) {
                    CONSUME_SPACE(false, false)
                    if (seq.current() == '|' || seq.current() == '/' || helper::match_eol<false>(seq)) {
                        break;
                    }
                    if (!and_) {
                        and_ = make_and<Input, String, Kind, Vec>();
                    }
                }
                return root;
            }

            template <class Input, class String, class Kind, template <class...> class Vec, class Src, class Fn>
            wrap::shared_ptr<Parser<Input, String, Kind, Vec>> compile_oneline(auto& tok, Sequencer<Src>& seq, Fn&& fn) {
                using parser_t = wrap::shared_ptr<Parser<Input, String, Kind, Vec>>;
                using and_t = wrap::shared_ptr<AndParser<Input, String, Kind, Vec>>;
                using or_t = wrap::shared_ptr<OrParser<Input, String, Kind, Vec>>;

                parser_t res, norm;
                and_t and_;
                or_t or_;
                while (true) {
                    CONSUME_SPACE(false, false)
                    if (seq.consume_if('/')) {
                        if (!or_) {
                            auto kd = fn(tok, KindMap::or_);
                            or_ = make_and<Input, String, Kind, Vec>(Vec<parser_t>{res}, tok, kd);
                            res = or_;
                        }
                        if (and_) {
                            or_->add_parser(std::move(and_));
                        }
                        else {
                            or_->add_parser(std::move(res));
                        }
                    }
                }
                return res;
            }
        }  // namespace internal

        template <class Input, class String, class Kind, template <class...> class Vec, class Src, class Fn = decltype(internal::def_Fn<Kind>())>
        wrap::shared_ptr<Parser<Input, String, Kind, Vec>> compile_parser(Sequencer<Src>& seq, Fn&& fn = internal::def_Fn<Kind>()) {
            //wrap::map<String, parser_t> desc;
            CONSUME_SPACE(true, true)
            String tok;
            if (!helper::read_whilef<true>(tok, seq, [&](auto&& c) {
                    return c != ':' && !helper::space::match_space(seq, true);
                })) {
                return nullptr;
            }
            CONSUME_SPACE(true, true)
            if (!seq.seek_if(":=")) {
                return nullptr;
            }
            CONSUME_SPACE(true, true)
            return nullptr;
        }
#undef CONSUME_SPACE
    }  // namespace parser
}  // namespace utils
