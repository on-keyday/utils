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
#include "../helper/iface_cast.h"

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

            template <class Input, class String, class Kind, template <class...> class Vec>
            struct FatalFunc {
                wrap::shared_ptr<Parser<Input, String, Kind, Vec>> subparser;
                bool operator()(auto& seq, auto& tok, auto& flag, auto& pos) {
                    auto b = pos;
                    auto tmp = subparser->parse(seq, pos);
                    if (!tmp.tok || tmp.fatal) {
                        flag = -1;
                        return false;
                    }
                    flag = 2;
                    tok = tmp.tok;
                    return true;
                }
            };

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
                    auto tmp = make_func<Input, String, Kind, Vec>(FatalFunc<Input, String, Kind, Vec>{}, kd);
                    v = tmp;
                }
                if (repeat) {
                    auto kd = fn("(repeat)", KindMap::repeat);
                    auto tmp = make_repeat<Input, String, Kind, Vec>(v, "(repeat)", kd);
                    v = tmp;
                }
                if (allow_none) {
                    auto kd = fn("(allow_none)", KindMap::allow_none);
                    auto tmp = make_allownone<Input, String, Kind, Vec>(v);
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
            struct WeakRef : Parser<Input, String, Kind, Vec> {
                Kind kind;
                String tok;
                wrap::weak_ptr<Parser<Input, String, Kind, Vec>> subparser;

                ParseResult<String, Kind, Vec> parse(Sequencer<Input>& seq, Pos& pos) override {
                    auto l = subparser.lock();
                    if (!l) {
                        return {.fatal = true};
                    }
                    return l->parse(seq, pos);
                }

                ParserKind declkind() const override {
                    return ParserKind::weak_ref;
                }
            };

            template <class Input, class String, class Kind, template <class...> class Vec, class Src, class Fn>
            wrap::shared_ptr<WeakRef<Input, String, Kind, Vec>> read_anyother(Sequencer<Src>& seq, Fn&& fn) {
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
                auto v = wrap::make_shared<WeakRef<Input, String, Kind, Vec>>();
                v->tok = std::move(tok);
                v->kind = kd;
                return v;
            }

            template <class Kind>
            constexpr auto def_Fn() {
                return [](auto&&, auto&&) { return Kind{}; };
            }

            template <class Input, class String, class Kind, template <class...> class Vec, class Src, class Fn>
            wrap::shared_ptr<Parser<Input, String, Kind, Vec>> compile_some(auto& tok, Sequencer<Src>& seq, Fn&& fn);

            template <class Input, class String, class Kind, template <class...> class Vec, class Src, class Fn>
            wrap::shared_ptr<Parser<Input, String, Kind, Vec>> compile_primary(auto& tok, Sequencer<Src>& seq, Fn&& fn) {
                auto wrap_code = [&](auto& p) {
                    return wrap_elm<Input, String, Kind, Vec>(p, seq, fn);
                };
                CONSUME_SPACE(false, false)
                if (auto p = read_str<Input, String, Kind, Vec>(seq, fn)) {
                    return wrap_code(p);
                }
                else if (auto p2 = read_anyother<Input, String, Kind, Vec>(seq, fn)) {
                    return wrap_code(p2);
                }
                else if (seq.consume_if('[')) {
                    auto se = compile_some<Input, String, Kind, Vec>(tok, seq, fn);
                    if (!se || !seq.consume_if(']')) {
                        return nullptr;
                    }
                    return wrap_code(se);
                }
                else {
                    return nullptr;
                }
            }

            template <class Input, class String, class Kind, template <class...> class Vec, class Src, class Fn>
            wrap::shared_ptr<Parser<Input, String, Kind, Vec>> compile_seq(auto& tok, Sequencer<Src>& seq, Fn&& fn) {
                auto root = compile_primary<Input, String, Kind, Vec>(tok, seq, fn);
                if (!root) {
                    return nullptr;
                }
                using parser_t = wrap::shared_ptr<Parser<Input, String, Kind, Vec>>;
                using and_t = wrap::shared_ptr<AndParser<Input, String, Kind, Vec>>;
                and_t and_;
                while (!seq.eos()) {
                    CONSUME_SPACE(false, false)
                    if (seq.current() == ']' || seq.current() == '|' || seq.current() == '/' || helper::match_eol<false>(seq)) {
                        break;
                    }
                    if (!and_) {
                        auto kd = fn(tok, KindMap::and_);
                        and_ = make_and<Input, String, Kind, Vec>(Vec<parser_t>{root}, tok, kd);
                        root = and_;
                    }
                    auto tmp = compile_primary<Input, String, Kind, Vec>(tok, seq, fn);
                    if (!tmp) {
                        return nullptr;
                    }
                    and_->add_parser(tmp);
                }
                return root;
            }

            template <class Input, class String, class Kind, template <class...> class Vec, class Src, class Fn>
            wrap::shared_ptr<Parser<Input, String, Kind, Vec>> compile_or(auto& tok, Sequencer<Src>& seq, Fn&& fn) {
                auto root = compile_seq<Input, String, Kind, Vec>(tok, seq, fn);
                if (!root) {
                    return nullptr;
                }
                using parser_t = wrap::shared_ptr<Parser<Input, String, Kind, Vec>>;
                using or_t = wrap::shared_ptr<OrParser<Input, String, Kind, Vec>>;
                or_t or_;
                while (!seq.eos()) {
                    CONSUME_SPACE(false, false)
                    if (seq.current() != '/' || seq.current() == ']' || helper::match_eol<false>(seq)) {
                        break;
                    }
                    if (!seq.consume_if('|')) {
                        return nullptr;
                    }
                    if (!or_) {
                        or_ = make_or<Input, String, Kind, Vec>(Vec<parser_t>{root});
                        root = or_;
                    }
                    auto tmp = compile_seq<Input, String, Kind, Vec>(tok, seq, fn);
                    if (!tmp) {
                        return nullptr;
                    }
                    or_->add_parser(tmp);
                }
                return root;
            }

            template <class Input, class String, class Kind, template <class...> class Vec, class Src, class Fn>
            wrap::shared_ptr<Parser<Input, String, Kind, Vec>> compile_some(auto& tok, Sequencer<Src>& seq, Fn&& fn) {
                auto root = compile_or<Input, String, Kind, Vec>(tok, seq, fn);
                if (!root) {
                    return nullptr;
                }
                using parser_t = wrap::shared_ptr<Parser<Input, String, Kind, Vec>>;
                using or_t = wrap::shared_ptr<SomePatternParser<Input, String, Kind, Vec>>;
                or_t or_;
                while (!seq.eos()) {
                    CONSUME_SPACE(false, false)
                    if (seq.current() == ']' || helper::match_eol<false>(seq)) {
                        break;
                    }
                    if (!seq.consume_if('/')) {
                        return nullptr;
                    }
                    if (!or_) {
                        auto kd = fn(tok, KindMap::or_);
                        or_ = make_somepattern<Input, String, Kind, Vec>(Vec<parser_t>{root}, tok, kd);
                        root = or_;
                    }
                    auto tmp = compile_or<Input, String, Kind, Vec>(tok, seq, fn);
                    if (!tmp) {
                        return nullptr;
                    }
                    or_->add_parser(tmp);
                }
                return root;
            }

            template <class Input, class String, class Kind, template <class...> class Vec, class Src, class Fn>
            wrap::shared_ptr<Parser<Input, String, Kind, Vec>> compile_oneline(auto& tok, Sequencer<Src>& seq, Fn&& fn) {
                auto res = compile_some<Input, String, Kind, Vec>(tok, seq, fn);
                if (!seq.eos() && !helper::match_eol(seq)) {
                    return nullptr;
                }
                return res;
            }

            template <class Input, class String, class Kind, template <class...> class Vec>
            bool replace_weakref(auto& tok, auto& mp) {
                auto rep_each = [&](auto ptr) {
                    for (auto& v : ptr->subparser) {
                        if (!replace_weakref<Input, String, Kind, Vec>(v, mp)) {
                            return false;
                        }
                    }
                    return true;
                };
                if (tok->declkind() == ParserKind::or_) {
                    auto ref = static_cast<OrParser<Input, String, Kind, Vec>*>(&*tok);
                    return rep_each(ref);
                }
                else if (tok->declkind() == ParserKind::and_) {
                    auto ref = static_cast<AndParser<Input, String, Kind, Vec>*>(&*tok);
                    return rep_each(ref);
                }
                else if (tok->declkind() == ParserKind::weak_ref) {
                    auto ref = static_cast<WeakRef<Input, String, Kind, Vec>*>(&*tok);
                    auto found = mp.find(ref->tok);
                    if (found != mp.end()) {
                        return false;
                    }
                    ref->subparser = std::get<1>(*found);
                }
                else if (tok->declkind() == ParserKind::some_pattern) {
                    auto ref = static_cast<SomePatternParser<Input, String, Kind, Vec>*>(&*tok);
                    return rep_each(ref);
                }
                else if (tok->declkind() == ParserKind::func) {
                    auto ref = static_cast<FuncParser<Input, String, Kind, Vec>*>(&*tok);
                    auto v = helper::iface_cast<FatalFunc<Input, String, Kind, Vec>>(&ref->func);
                    if (v) {
                        return replace_weakref<Input, String, Kind, Vec>(v->subparser, mp);
                    }
                }
                else if (tok->declkind() == ParserKind::allow_none) {
                    auto ref = static_cast<AllowNoneParser<Input, String, Kind, Vec>*>(&*tok);
                    return replace_weakref<Input, String, Kind, Vec>(ref->subparser, mp);
                }
                else if (tok->declkind() == ParserKind::repeat) {
                    auto ref = static_cast<RepeatParser<Input, String, Kind, Vec>*>(&*tok);
                    return replace_weakref<Input, String, Kind, Vec>(ref->subparser, mp);
                }
                return true;
            }

        }  // namespace internal

        template <class Input, class String, class Kind, template <class...> class Vec, class Src, class Fn = decltype(internal::def_Fn<Kind>())>
        wrap::shared_ptr<Parser<Input, String, Kind, Vec>> compile_parser(Sequencer<Src>& seq, Fn&& fn = internal::def_Fn<Kind>()) {
            using parser_t = wrap::shared_ptr<Parser<Input, String, Kind, Vec>>;
            wrap::map<String, parser_t> desc;
            while (!seq.eos()) {
                CONSUME_SPACE(true, true)
                String tok;
                if (!helper::read_whilef<true>(tok, seq, [&](auto&& c) {
                        return c != ':' && !helper::space::match_space(seq, true);
                    })) {
                    return nullptr;
                }
                if (desc.find(tok) != desc.end()) {
                    return nullptr;
                }
                auto& value = desc[tok];
                CONSUME_SPACE(true, true)
                if (!seq.seek_if(":=")) {
                    return nullptr;
                }
                CONSUME_SPACE(true, true)
                auto res = internal::compile_oneline<Input, String, Kind, Vec>(tok, seq, fn);
                if (!res) {
                    return nullptr;
                }
                value = res;
            }
            for (auto& v : desc) {
                if (!internal::replace_weakref<Input, String, Kind, Vec>(std::get<1>(v), desc)) {
                    return nullptr;
                }
            }
            return nullptr;
        }
#undef CONSUME_SPACE
    }  // namespace parser
}  // namespace utils
