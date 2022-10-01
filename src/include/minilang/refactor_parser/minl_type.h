/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// minl_type - type definition
#pragma once
#include "minlpass.h"
#include "minl_block.h"

namespace utils {
    namespace minilang {
        namespace parser {
            constexpr auto type_(auto struct_parse, auto func_parse) {
                auto f = [=](auto&& f, auto&& p) -> std::shared_ptr<TypeNode> {
                    MINL_FUNC_LOG("type_signature")
                    MINL_BEGIN_AND_START(p.seq);
                    auto self_call = [&]() {
                        return f(f, p);
                    };
                    auto idents = [&](auto word, auto id) -> std::shared_ptr<TypeNode> {
                        if (expect_ident(p.seq, word)) {
                            const auto end = p.seq.rptr;
                            auto base = self_call();
                            if (!base) {
                                p.err = true;
                                return nullptr;
                            }
                            auto tmp = std::make_shared<TypeNode>();
                            tmp->type = std::move(base);
                            tmp->str = id;
                            tmp->pos = {start, end};
                            return tmp;
                        }
                        return nullptr;
                    };
                    auto a_symbol = [&](auto word, auto id) -> std::shared_ptr<TypeNode> {
                        if (seq.seek_if(word)) {
                            const auto end = p.seq.rptr;
                            auto base = self_call();
                            if (!base) {
                                p.err = true;
                                return nullptr;
                            }
                            auto tmp = std::make_shared<TypeNode>();
                            tmp->type = std::move(base);
                            tmp->str = id;
                            tmp->pos = {start, end};
                            return tmp;
                        }
                        return nullptr;
                    };
                    p.err = false;
                    if (auto node = idents("mut", mut_str_); node || p.err) {
                        return node;
                    }
                    else if (auto node = idents("const", const_str_); node || p.err) {
                        return node;
                    }
                    else if (expect_ident(p.seq, "typeof")) {
                        helper::space::consume_space(p.seq, true);
                        if (!p.seq.seek_if("(")) {
                            p.errc.say("expect type typeof() begin ( but not");
                            p.errc.trace(start, p.seq);
                            p.err = true;
                            return true;
                        }
                        auto tmp = p.expr(p);
                        if (!tmp) {
                            p.errc.say("expect typeof() expression but not");
                            p.errc.trace(start, p.seq);
                            p.err = true;
                            return true;
                        }
                        helper::space::consume_space(p.seq, true);
                        if (!p.seq.seek_if(")")) {
                            p.errc.say("expect type typeof() end ) but not");
                            p.errc.trace(start, p.seq);
                            p.err = true;
                            return true;
                        }
                        auto ret = std::make_shared<ArrayTypeNode>();
                        ret->expr = std::move(tmp);
                        ret->str = typeof_str_;
                        ret->pos = {start, p.seq.rptr};
                        node = std::move(ret);
                        return true;
                    }
                    else if (expect_ident(p.seq, "genr")) {
                        const auto end = p.seq.rptr;
                        helper::space::consume_space(p.seq, true);
                        if (!seq.seek_if("(")) {
                            seq.rptr = end;
                            auto gen = std::make_shared<GenericTypeNode>();
                            gen->str = genr_str_;
                            gen->pos = {start, end};
                            node = std::move(gen);
                            return true;
                        }
                        helper::space::consume_space(p.seq, true);
                        std::string tp;
                        if (!ident_default_read(tp, p.seq)) {
                            p.errc.say("expect type parameter identifier but not");
                            p.errc.trace(start, p.seq);
                            p.err = true;
                            return true;
                        }
                        helper::space::consume_space(p.seq, true);
                        if (p.seq.seek_if(")")) {
                            p.errc.say("expect type parameter end ) but not");
                            p.errc.trace(start, p.seq);
                            p.err = true;
                            return true;
                        }
                        auto gen = std::make_shared<GenericTypeNode>();
                        gen->str = genr_str_;
                        gen->pos = {start, end};
                        gen->type_param = std::move(tp);
                        return gen;
                    }
                    else if (seq.seek_if("...")) {
                        const auto end = seq.rptr;
                        helper::space::consume_space(p.seq, true);
                        std::shared_ptr<TypeNode> node;
                        if (!p.seq.match(")")) {
                            node = self_call();
                            if (!node) {
                                p.err = true;
                                return nullptr;
                            }
                        }
                        auto tmp = std::make_shared<TypeNode>();
                        tmp->type = std::static_pointer_cast<TypeNode>(node);
                        tmp->str = va_arg_str_;
                        tmp->pos = {start, end};
                        node = std::move(tmp);
                        return true;
                    }
                    else if (auto node = a_symbol("*", pointer_str_); node || p.err) {
                        return node;
                    }
                    else if (auto node = a_symbol("&", reference_str_); node || p.err) {
                        return node;
                    }
                    else if (p.seq.seek_if("[")) {
                        helper::space::consume_space(p.seq, true);
                        const char* type_ = array_str_;
                        if (p.seq.seek_if("]")) {
                            type_ = vector_str_;
                        }
                        else {
                            node = p.expr(p);
                            if (!node) {
                                p.err = true;
                                return nullptr;
                            }
                            helper::space::consume_space(p.seq, true);
                            if (!p.seq.seek_if("]")) {
                                p.errc.say("expected array end ] but not");
                                p.errc.trace(start, p.seq);
                                p.err = true;
                                return true;
                            }
                        }
                        const auto end = p.seq.rptr;
                        auto tmp = self_call();
                        if (!tmp) {
                            err = true;
                            return true;
                        }
                        auto ret = std::make_shared<ArrayTypeNode>();
                        ret->type = std::static_pointer_cast<TypeNode>(tmp);
                        ret->expr = std::move(node);
                        ret->str = type_;
                        ret->pos = {start, end};
                        node = std::move(ret);
                        return true;
                    }
                    else if (auto node = struct_parse(p); node || p.err) {
                        return node;
                    }
                    else if (auto node = func_parse(p); node || p.err) {
                        return node;
                    }
                    else {
                        std::string str;
                        while (true) {
                            if (!ident_default_read(str, p.seq)) {
                                p.errc.say("expected type identifier but not");
                                p.errc.trace(start, p.seq);
                                p.err = true;
                                return nullptr;
                            }
                            const auto end = p.seq.rptr;
                            helper::space::consume_space(p.seq, false);
                            if (p.seq.consume_if('.')) {
                                str.push_back('.');
                                helper::space::consume_space(p.seq, true);
                                continue;
                            }
                            p.seq.rptr = end;
                            break;
                        }
                        auto tmp = std::make_shared<TypeNode>();
                        tmp->str = std::move(str);
                        tmp->pos = {start, seq.rptr};
                        return tmp;
                    }
                };
                return [=](auto&& p) {
                    return f(f, p);
                };
            }

            constexpr auto typedef_() {
                return [](auto&& p) -> std::shared_ptr<TypeDefNode> {
                    MINL_FUNC_LOG("type_define")
                    MINL_BEGIN_AND_START(p.seq);
                    if (!expect_ident(p.seq, "type")) {
                        return false;
                    }
                    helper::space::consume_space(p.seq, true);
                    if (p.seq.match("<")) {  // type primitive
                        p.seq.rptr = start;
                        return false;
                    }
                    std::shared_ptr<TypeDefNode> root, curnode;
                    auto read_types = [&] {
                        const char* type_ = new_type_str_;
                        constexpr auto read = ident_default_read;
                        std::string str;
                        const auto name_begin = p.seq.rptr;
                        if (!read(str, p.seq)) {
                            p.errc.say("expected new type name but not");
                            p.errc.trace(start, p.seq);
                            p.err = true;
                            return true;
                        }
                        const auto name_end = p.seq.rptr;
                        helper::space::consume_space(p.seq, true);
                        if (p.seq.seek_if("=")) {
                            type_ = type_alias_str_;
                            helper::space::consume_space(p.seq, true);
                        }
                        auto node = p.type(p);
                        if (!node) {
                            p.errc.say("expected type signature but not");
                            p.errc.trace(start, p.seq);
                            p.err = true;
                            return true;
                        }
                        auto tmp = std::make_shared<TypeDefNode>();
                        tmp->str = type_;
                        tmp->ident = make_ident_node(str, {name_begin, name_end});
                        tmp->type = std::static_pointer_cast<TypeNode>(node);
                        tmp->pos = {name_begin, p.seq.rptr};
                        curnode->next = tmp;
                        curnode = tmp;
                        return true;
                    };
                    root = std::make_shared<TypeDefNode>();
                    root->str = type_group_str_;
                    curnode = root;
                    p.err = false;
                    if (p.seq.seek_if("(")) {
                        while (true) {
                            helper::space::consume_space(p.seq, true);
                            if (p.seq.seek_if(")")) {
                                break;
                            }
                            read_types();
                            if (p.err) {
                                return nullptr;
                            }
                        }
                    }
                    else {
                        read_types();
                    }
                    if (p.err) {
                        return true;
                    }
                    root->pos = {start, p.seq.rptr};
                    return root;
                };
            }

            constexpr auto type_primitive() {
                return [](auto&& p) -> std::shared_ptr<TypeNode> {
                    MINL_FUNC_LOG("type_primitive")
                    MINL_BEGIN_AND_START(p.seq);
                    std::shared_ptr<MinNode> tmp;
                    if (expect_ident(p.seq, "type")) {
                        helper::space::consume_space(p.seq, true);
                        if (!p.seq.seek_if("<")) {
                            // TODO(on-keyday): error?
                            p.errc.say("expect type primitive begin < but not");
                            p.errc.trace(start, p.seq);
                            p.err = true;
                            return nullptr;
                        }
                        p.err = false;
                        tmp = p.type(p);
                        if (!tmp) {
                            p.errc.say("expect type primitive type signature but not");
                            p.errc.trace(start, p.seq);
                            p.err = true;
                            return nullptr;
                        }
                        helper::space::consume_space(p.seq, true);
                        if (!p.seq.seek_if(">")) {
                            // TODO(on-keyday): error?
                            p.errc.say("expect type primitive end > but not");
                            p.errc.trace(start, p.seq);
                            p.err = true;
                            return nullptr;
                        }
                    }
                    else if (match_ident(p.seq, "struct") || match_ident(p.seq, "union") ||
                             match_ident(p.seq, "interface")) {
                        p.err = false;
                        tmp = p.type(p);
                        if (!tmp) {
                            p.errc.say("expect type primitive type signature but not");
                            p.errc.trace(start, p.seq);
                            p.err = true;
                            return nullptr;
                        }
                    }
                    else {
                        p.seq.rptr = begin;
                        return nullptr;
                    }
                    auto res = std::make_shared<TypeNode>();
                    res->str = typeprim_str_;
                    res->type = std::static_pointer_cast<TypeNode>(tmp);
                    res->pos = {start, p.seq.rptr};
                    return res;
                };
            }
        }  // namespace parser
    }      // namespace minilang
}  // namespace utils
