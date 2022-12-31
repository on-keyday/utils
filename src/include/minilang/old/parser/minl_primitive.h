/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// minlprimitive - minilang primitive parser
#pragma once
#include "minlpass.h"
#include <escape/read_string.h>

namespace utils {
    namespace minilang {
        namespace parser {

            constexpr auto always_error(const char* msg = "unexpected invocation. this is library bug") {
                return [=](auto&& p) {
                    p.errc.say(msg);
                    p.errc.trace(p.seq.rptr, p.seq);
                    p.err = true;
                    return nullptr;
                };
            }

            constexpr auto number(bool allow_float) {
                return [=](auto&& p) -> std::shared_ptr<NumberNode> {
                    MINL_FUNC_LOG("number")
                    MINL_BEGIN_AND_START(p.seq);
                    if (!number::is_digit(p.seq.current())) {
                        p.seq.rptr = begin;
                        return nullptr;
                    }
                    std::string str;
                    int prefix = 10;
                    bool is_float = false;
                    bool* flptr = allow_float ? &is_float : nullptr;
                    if (!number::read_prefixed_number(p.seq, str, &prefix, flptr)) {
                        p.errc.say("expect number but not");
                        p.errc.trace(start, p.seq);
                        p.err = true;
                        return nullptr;
                    }
                    auto node = std::make_shared<NumberNode>();
                    node->is_float = is_float;
                    node->radix = prefix;
                    node->pos = {start, p.seq.rptr};
                    node->str = std::move(str);
                    number::NumConfig conf{number::default_num_ignore()};
                    conf.offset = prefix == 10 ? 0 : 2;
                    if (is_float) {
                        node->parsable = number::parse_float(node->str, node->floating, node->radix, conf);
                    }
                    else {
                        node->parsable = number::parse_integer(node->str, node->integer, node->radix, conf);
                    }
                    return node;
                };
            }

            template <class Node = NumberNode>
            constexpr auto radix_integer(int radix, auto errmsg) {
                return [=](auto&& p) -> std::shared_ptr<Node> {
                    MINL_FUNC_LOG("radix_integer");
                    MINL_BEGIN_AND_START(p.seq);
                    if (!number::is_digit(p.seq.current())) {
                        p.seq.rptr = begin;
                        return nullptr;
                    }
                    std::string num;
                    if (!number::read_number(num, p.seq, radix)) {
                        p.errc.say(errmsg);
                        p.errc.trace(start, p.seq);
                        p.err = true;
                        return nullptr;
                    }
                    auto node = std::make_shared<Node>();
                    node->radix = radix;
                    node->pos = {start, p.seq.rptr};
                    node->str = std::move(num);
                    node->is_float = false;
                    node->parsable = number::parse_integer(node->str, node->integer, radix);
                    return node;
                };
            }

            constexpr auto string(bool allow_multi_line) {
                return [=](auto&& p) -> std::shared_ptr<StringNode> {
                    MINL_FUNC_LOG("string")
                    MINL_BEGIN_AND_START(p.seq);
                    std::string str;
                    if (p.seq.current() == '"' || p.seq.current() == '\'') {
                        auto pf = p.seq.current();
                        if (!escape::read_string(str, p.seq, escape::ReadFlag::parser_mode,
                                                 escape::c_prefix())) {
                            p.errc.say("expect string but not");
                            p.errc.trace(start, p.seq);
                            p.err = true;
                            return nullptr;
                        }
                        auto node = std::make_shared<StringNode>();
                        node->pos = {start, p.seq.rptr};
                        node->prefix = pf;
                        node->store_c = 0;
                        node->str = std::move(str);
                        return node;
                    }
                    else if (allow_multi_line && p.seq.current() == '`') {
                        auto c = 0;
                        size_t count = 0;
                        if (!escape::varialbe_prefix_string(str, p.seq, escape::single_char_varprefix('`', c, count))) {
                            p.errc.say("expect raw string but not");
                            p.errc.trace(start, p.seq);
                            return nullptr;
                        }
                        auto node = std::make_shared<StringNode>();
                        node->pos = {start, p.seq.rptr};
                        node->prefix = '`';
                        node->store_c = c;
                        node->str = std::move(str);
                        return node;
                    }
                    p.seq.rptr = begin;
                    return nullptr;
                };
            }

            template <class Fn = decltype(ident_default_read)>
            constexpr auto ident(Fn ident_read = ident_default_read) {
                return [=](auto&& p) -> std::shared_ptr<MinNode> {
                    MINL_FUNC_LOG("ident")
                    MINL_BEGIN_AND_START(p.seq);
                    std::string str;
                    if (!ident_read(str, p.seq)) {
                        p.seq.rptr = begin;
                        return nullptr;
                    }
                    auto node = std::make_shared<MinNode>();
                    node->str = std::move(str);
                    node->pos = {start, p.seq.rptr};
                    return node;
                };
            }

            constexpr auto or_(auto... op) {
                return [=](auto&& p) -> std::shared_ptr<MinNode> {
                    MINL_FUNC_LOG("or_op");
                    std::shared_ptr<MinNode> node;
                    auto fold = [&](auto& op) {
                        p.err = false;
                        node = op(p);
                        if (!node) {
                            if (p.err) {
                                return true;
                            }
                            return false;
                        }
                        return true;
                    };
                    (... || fold(op));
                    return node;
                };
            }

            template <class Fn = decltype(ident_default_read)>
            constexpr auto ident_or_err(Fn ident_read = ident_default_read) {
                return or_(ident(ident_read), always_error("expect identifier but not"));
            }

            constexpr auto filter_passloc(auto inner, pass_loc loc) {
                return [=](auto&& p) {
                    if (p.loc != loc) {
                        return decltype(inner(p))(nullptr);
                    }
                    return inner(p);
                };
            }

            constexpr auto branch_passloc(auto then, auto els, pass_loc loc) {
                return [=](auto&& p) -> std::shared_ptr<MinNode> {
                    if (p.loc == loc) {
                        return then(p);
                    }
                    return els(p);
                };
            }

        }  // namespace parser
    }      // namespace minilang
}  // namespace utils
