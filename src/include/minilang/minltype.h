/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// minlstruct - minilang struct parser
#pragma once
#include "minl.h"

namespace utils {
    namespace minilang {
        /*
         struct {
            *[Name Type ["=" Expr]]
         }

         type Name "="? Type

         *["*"|"["Expr?"]"] Name

        */

        constexpr auto type_group_str_ = "(type_group)";
        constexpr auto new_type_str_ = "(new_type)";
        constexpr auto type_alias_str_ = "(type_alias)";

        constexpr auto type_siganture = "(type_signature)";
        constexpr auto pointer_str_ = "(pointer)";
        constexpr auto reference_str_ = "(reference)";
        constexpr auto array_str_ = "[(fixed)]";
        constexpr auto vector_str_ = "[(dynamic)]";
        constexpr auto mut_str_ = "(mutable)";
        constexpr auto const_str_ = "(const)";
        constexpr auto va_arg_str_ = "(variable_argument)";
        constexpr auto typeof_str_ = "(typeof)";
        constexpr auto genr_str_ = "(generic)";

        constexpr auto struct_str_ = "{(struct)}";
        constexpr auto struct_field_str_ = "(struct_field)";
        constexpr auto union_str_ = "{(union)}";
        constexpr auto union_field_str_ = "(union_field)";

        struct TypeNode : MinNode {
            std::shared_ptr<TypeNode> type;
            MINL_Constexpr TypeNode()
                : MinNode(nt_type) {}

           protected:
            MINL_Constexpr TypeNode(int id)
                : MinNode(id) {}
        };

        // Derive TypeNode
        struct ArrayTypeNode : TypeNode {
            std::shared_ptr<MinNode> expr;
            MINL_Constexpr ArrayTypeNode()
                : TypeNode(nt_arrtype) {
            }
        };

        // Derive TypeNode
        struct GenericTypeNode : TypeNode {
            MINL_Constexpr GenericTypeNode()
                : TypeNode(nt_gentype) {}
            std::string type_param;
        };

        // Derive TypeNode
        struct StructFieldNode : TypeNode {
            std::shared_ptr<StructFieldNode> next;
            std::shared_ptr<MinNode> ident;
            std::shared_ptr<MinNode> init;
            MINL_Constexpr StructFieldNode()
                : TypeNode(nt_struct_field) {}
        };

        constexpr bool match_typeprefix(auto& seq) {
            return seq.match("*") || seq.match("[") || seq.match("&") ||
                   match_ident(seq, "struct") || match_ident(seq, "union") || match_ident(seq, "fn") ||
                   match_ident(seq, "mut") || match_ident(seq, "const") || match_ident(seq, "genr") ||
                   match_ident(seq, "interface") || match_ident(seq, "typeof") || seq.match(")") ||
                   seq.match("...");
        }

        constexpr auto idents(auto& str) {
            return helper::make_ref_splitview(str, ",");
        }

        constexpr auto read_parameter(auto& seq, auto& param_end, auto& param_name, bool& no_param_name) {
            const auto param_start = seq.rptr;
            while (true) {
                // (*Arg,ident Arg) -> ok (type,name type)
                // (Arg1,Arg2,Arg3) -> ok (type,type,type)
                // (ident1,ident2,ident3 Arg) -> ok (name,name,name type)
                // (Arg,ident Arg) -> ok but... (name,name type)
                // (type Arg,ident Arg) -> ok (type,ident type)
                // (Arg,type Arg,Arg) -> ok (type,type,type)
                // (type Arg,Arg,Arg) -> ok (type,type,type)
                // (Arg,Arg,Arg type Arg) -> ok (name,name,name type)
                // (ident,type ident Arg) -> bad (name,type? type?)
                // (ident.Type) -> ok (type)
                // (ident user.Type,user.Type) -> ok (ident type,type)
                // (Type,Type,user.Type) -> ok (type,type,type)
                if (match_typeprefix(seq)) {
                    seq.rptr = param_start;
                    no_param_name = true;
                    break;
                }
                if (expect_ident(seq, "type")) {
                    if (param_name.size()) {
                        seq.rptr = param_start;
                    }
                    no_param_name = true;
                    break;
                }
                if (!ident_default_read(param_name, seq)) {
                    return false;
                }
                param_end = seq.rptr;
                helper::space::consume_space(seq, true);
                if (seq.seek_if(",")) {
                    // expect multiple param_name in same type
                    param_name.push_back(',');
                    helper::space::consume_space(seq, true);
                    continue;
                }
                if (seq.match(")")) {
                    seq.rptr = param_start;
                    no_param_name = true;
                    break;
                }
                auto tmp = seq.rptr;
                if (seq.seek_if(".") && (helper::space::consume_space(seq, true), ident_default()(seq))) {
                    seq.rptr = param_start;
                    no_param_name = true;
                    break;
                }
                seq.rptr = tmp;
                break;
            }
            if (expect_ident(seq, "type")) {
                helper::space::consume_space(seq, true);
            }
            return true;
        }

        constexpr auto struct_signature() {
            return [](auto&& type_, auto&& stat_, auto&& expr, auto& seq, std::shared_ptr<MinNode>& node, bool& err, auto& errc) -> bool {
                MINL_FUNC_LOG("struct_signature")
                const auto begin = seq.rptr;
                helper::space::consume_space(seq, true);
                const auto start = seq.rptr;
                const char* str_ = nullptr;
                const char* field_str_ = nullptr;
                const char* word = nullptr;
                if (expect_ident(seq, "struct")) {
                    str_ = struct_str_;
                    field_str_ = struct_field_str_;
                    word = "struct";
                }
                else if (expect_ident(seq, "union")) {
                    str_ = union_str_;
                    field_str_ = union_field_str_;
                    word = "union";
                }
                else {
                    seq.rptr = begin;
                    return false;
                }
                helper::space::consume_space(seq, true);
                if (!seq.seek_if("{")) {
                    errc.say("expected ", word, " begin { but not");
                    errc.trace(start, seq);
                    err = true;
                    return true;
                }
                auto root = std::make_shared<StructFieldNode>();
                root->str = str_;
                auto cur = root;
                while (true) {
                    helper::space::consume_space(seq, true);
                    if (seq.eos()) {
                        errc.say("unexpected eof at reading ", word, " field");
                        errc.trace(start, seq);
                        err = true;
                        return true;
                    }
                    if (seq.seek_if("}")) {
                        break;
                    }
                    std::string name;
                    const auto field_start = seq.rptr;
                    auto name_end = field_start;
                    while (true) {
                        if (!ident_default_read(name, seq)) {
                            errc.say("expected ", word, " field identifier but not");
                            errc.trace(start, seq);
                            err = true;
                            return true;
                        }
                        name_end = seq.rptr;
                        helper::space::consume_space(seq, false);
                        if (seq.seek_if(",")) {
                            name.push_back(',');
                            helper::space::consume_space(seq, true);
                            continue;
                        }
                        break;
                    }
                    std::shared_ptr<MinNode> tmp;
                    if (!type_(stat_, expr, seq, tmp, err, errc) || err) {
                        errc.say("expected ", word, " field type but not");
                        errc.trace(start, seq);
                        err = true;
                        return true;
                    }
                    auto ty = std::static_pointer_cast<TypeNode>(tmp);
                    tmp = nullptr;
                    auto field_end = seq.rptr;
                    helper::space::consume_space(seq, false);
                    if (seq.seek_if("=")) {
                        helper::space::consume_space(seq, true);
                        tmp = expr(seq);
                        if (!tmp) {
                            errc.say("expected ", word, " field initialization expr but not");
                            errc.trace(start, seq);
                            err = true;
                            return true;
                        }
                        field_end = seq.rptr;
                    }
                    auto field = std::make_shared<StructFieldNode>();
                    field->str = field_str_;
                    field->pos = {field_start, field_end};
                    auto name_node = std::make_shared<MinNode>();
                    name_node->str = std::move(name);
                    name_node->pos = {field_start, name_end};
                    field->ident = std::move(name_node);
                    field->type = std::move(ty);
                    field->init = std::move(tmp);
                    cur->next = field;
                    cur = field;
                }
                node = std::move(root);
                node->pos = {start, seq.rptr};
                return true;
            };
        }

        constexpr auto type_signatures(auto&& struct_parse, auto&& func_parse) {
            auto f = [=](auto&& f, auto&& stat_, auto&& expr, auto& seq, std::shared_ptr<MinNode>& node, bool& err, auto& errc) -> bool {
                MINL_FUNC_LOG("type_signature")
                helper::space::consume_space(seq, true);
                const auto start = seq.rptr;
                auto self_call = [&](auto&& stat_, auto&& expr, auto& seq, auto& node, bool& err, auto& errc) {
                    return f(f, stat_, expr, seq, node, err, errc);
                };
                auto idents = [&](auto word, auto id) {
                    if (expect_ident(seq, word)) {
                        const auto end = seq.rptr;
                        if (!self_call(stat_, expr, seq, node, err, errc) || err) {
                            err = true;
                            return true;
                        }
                        auto tmp = std::make_shared<TypeNode>();
                        tmp->type = std::static_pointer_cast<TypeNode>(node);
                        tmp->str = id;
                        tmp->pos = {start, end};
                        node = std::move(tmp);
                        return true;
                    }
                    return false;
                };
                auto a_symbol = [&](auto word, auto id) {
                    if (seq.seek_if(word)) {
                        const auto end = seq.rptr;
                        if (!self_call(stat_, expr, seq, node, err, errc) || err) {
                            err = true;
                            return true;
                        }
                        auto tmp = std::make_shared<TypeNode>();
                        tmp->type = std::static_pointer_cast<TypeNode>(node);
                        tmp->str = id;
                        tmp->pos = {start, end};
                        node = std::move(tmp);
                        return true;
                    }
                    return false;
                };
                if (idents("mut", mut_str_)) {
                    return true;
                }
                else if (idents("const", const_str_)) {
                    return true;
                }
                else if (expect_ident(seq, "typeof")) {
                    helper::space::consume_space(seq, true);
                    if (!seq.seek_if("(")) {
                        errc.say("expect type typeof() begin ( but not");
                        errc.trace(start, seq);
                        err = true;
                        return true;
                    }
                    auto tmp = expr(seq);
                    if (!tmp) {
                        errc.say("expect typeof() expression but not");
                        errc.trace(start, seq);
                        err = true;
                        return true;
                    }
                    helper::space::consume_space(seq, true);
                    if (!seq.seek_if(")")) {
                        errc.say("expect type typeof() end ) but not");
                        errc.trace(start, seq);
                        err = true;
                        return true;
                    }
                    auto ret = std::make_shared<ArrayTypeNode>();
                    ret->expr = std::move(tmp);
                    ret->str = typeof_str_;
                    ret->pos = {start, seq.rptr};
                    node = std::move(ret);
                    return true;
                }
                else if (expect_ident(seq, "genr")) {
                    const auto end = seq.rptr;
                    helper::space::consume_space(seq, true);
                    if (!seq.seek_if("(")) {
                        seq.rptr = end;
                        auto gen = std::make_shared<GenericTypeNode>();
                        gen->str = genr_str_;
                        gen->pos = {start, end};
                        node = std::move(gen);
                        return true;
                    }
                    helper::space::consume_space(seq, true);
                    std::string tp;
                    if (!ident_default_read(tp, seq)) {
                        errc.say("expect type parameter identifier but not");
                        errc.trace(start, seq);
                        err = true;
                        return true;
                    }
                    helper::space::consume_space(seq, true);
                    if (seq.seek_if(")")) {
                        errc.say("expect type parameter end ) but not");
                        errc.trace(start, seq);
                        err = true;
                        return true;
                    }
                    auto gen = std::make_shared<GenericTypeNode>();
                    gen->str = genr_str_;
                    gen->pos = {start, end};
                    gen->type_param = std::move(tp);
                    node = std::move(gen);
                    return true;
                }
                else if (seq.seek_if("...")) {
                    const auto end = seq.rptr;
                    helper::space::consume_space(seq, true);
                    if (!seq.match(")")) {
                        if (!self_call(stat_, expr, seq, node, err, errc) || err) {
                            err = true;
                            return true;
                        }
                    }
                    auto tmp = std::make_shared<TypeNode>();
                    tmp->type = std::static_pointer_cast<TypeNode>(node);
                    tmp->str = va_arg_str_;
                    tmp->pos = {start, end};
                    node = std::move(tmp);
                    return true;
                }
                else if (a_symbol("*", pointer_str_)) {
                    return true;
                }
                else if (a_symbol("&", reference_str_)) {
                    return true;
                }
                else if (seq.seek_if("[")) {
                    helper::space::consume_space(seq, true);
                    const char* type_ = array_str_;
                    if (seq.seek_if("]")) {
                        type_ = vector_str_;
                    }
                    else {
                        node = expr(seq);
                        if (!node) {
                            err = true;
                            return true;
                        }
                        helper::space::consume_space(seq, true);
                        if (!seq.seek_if("]")) {
                            errc.say("expected array end ] but not");
                            errc.trace(start, seq);
                            err = true;
                            return true;
                        }
                    }
                    const auto end = seq.rptr;
                    std::shared_ptr<MinNode> tmp;
                    if (!self_call(stat_, expr, seq, tmp, err, errc) || err) {
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
                else if (struct_parse(self_call, stat_, expr, seq, node, err, errc)) {
                    if (err) {
                        errc.say("expected type struct/union/interface but not");
                        errc.trace(start, seq);
                    }
                    return true;
                }
                else if (func_parse(self_call, stat_, expr, seq, node, err, errc)) {
                    if (err) {
                        errc.say("expected type function but not");
                        errc.trace(start, seq);
                    }
                    return true;
                }
                else {
                    std::string str;
                    while (true) {
                        if (!ident_default_read(str, seq)) {
                            errc.say("expected type identifier but not");
                            errc.trace(start, seq);
                            err = true;
                            return true;
                        }
                        const auto end = seq.rptr;
                        helper::space::consume_space(seq, false);
                        if (seq.consume_if('.')) {
                            str.push_back('.');
                            helper::space::consume_space(seq, true);
                            continue;
                        }
                        seq.rptr = end;
                        break;
                    }
                    auto tmp = std::make_shared<TypeNode>();
                    tmp->str = std::move(str);
                    tmp->pos = {start, seq.rptr};
                    node = std::move(tmp);
                    return true;
                }
            };
            return [=](auto&& stat_, auto&& expr, auto& seq, std::shared_ptr<MinNode>& node, bool& err, auto& errc) {
                return f(f, stat_, expr, seq, node, err, errc);
            };
        }

        struct TypeDefNode : MinNode {
            std::shared_ptr<TypeDefNode> next;
            std::shared_ptr<MinNode> ident;
            std::shared_ptr<TypeNode> type;
            MINL_Constexpr TypeDefNode()
                : MinNode(nt_typedef) {}
        };

        constexpr auto type_define(auto&& type_parse) {
            return [=](auto&& stat_, auto&& expr, auto& seq, std::shared_ptr<MinNode>& node, bool& err, auto& errc) {
                MINL_FUNC_LOG("type_define")
                helper::space::consume_space(seq, true);
                const auto start = seq.rptr;
                if (!expect_ident(seq, "type")) {
                    return false;
                }
                helper::space::consume_space(seq, true);
                if (seq.match("<")) {  // type primitive
                    seq.rptr = start;
                    return false;
                }
                std::shared_ptr<TypeDefNode> root, curnode;
                auto read_types = [&] {
                    const char* type_ = new_type_str_;
                    constexpr auto read = ident_default_read;
                    std::string str;
                    const auto name_begin = seq.rptr;
                    if (!read(str, seq)) {
                        errc.say("expected new type name but not");
                        errc.trace(start, seq);
                        err = true;
                        return true;
                    }
                    const auto name_end = seq.rptr;
                    helper::space::consume_space(seq, true);
                    if (seq.seek_if("=")) {
                        type_ = type_alias_str_;
                        helper::space::consume_space(seq, true);
                    }
                    if (!type_parse(stat_, expr, seq, node, err, errc) || err) {
                        errc.say("expected type signature but not");
                        errc.trace(start, seq);
                        err = true;
                        return true;
                    }
                    auto tmp = std::make_shared<TypeDefNode>();
                    tmp->str = type_;
                    tmp->ident = make_ident_node(str, {name_begin, name_end});
                    tmp->type = std::static_pointer_cast<TypeNode>(node);
                    tmp->pos = {name_begin, seq.rptr};
                    curnode->next = tmp;
                    curnode = tmp;
                    return true;
                };
                root = std::make_shared<TypeDefNode>();
                root->str = type_group_str_;
                curnode = root;
                if (seq.seek_if("(")) {
                    while (true) {
                        helper::space::consume_space(seq, true);
                        if (seq.seek_if(")")) {
                            break;
                        }
                        read_types();
                        if (err) {
                            return true;
                        }
                    }
                }
                else {
                    read_types();
                }
                if (err) {
                    return true;
                }
                root->pos = {start, seq.rptr};
                node = std::move(root);
                return true;
            };
        }

        constexpr auto typeprim_str_ = "(type_primitive)";

        constexpr auto type_primitive(auto&& stats, auto&& type_parse) {
            return [=](auto&& expr, auto& seq, std::shared_ptr<MinNode>& node, bool& err, auto& errc) {
                MINL_FUNC_LOG("type_primitive")
                const auto begin = seq.rptr;
                helper::space::consume_space(seq, true);
                const auto start = seq.rptr;
                std::shared_ptr<MinNode> tmp;
                if (expect_ident(seq, "type")) {
                    helper::space::consume_space(seq, true);
                    if (!seq.seek_if("<")) {
                        // TODO(on-keyday): error?
                        errc.say("expect type primitive begin < but not");
                        errc.trace(start, seq);
                        err = true;
                        return true;
                    }
                    err = false;
                    if (!type_parse(stats, expr, seq, tmp, err, errc) || err) {
                        errc.say("expect type primitive type signature but not");
                        errc.trace(start, seq);
                        err = true;
                        return true;
                    }
                    helper::space::consume_space(seq, true);
                    if (!seq.seek_if(">")) {
                        // TODO(on-keyday): error?
                        errc.say("expect type primitive end > but not");
                        errc.trace(start, seq);
                        err = true;
                        return true;
                    }
                }
                else if (match_ident(seq, "struct") || match_ident(seq, "union") ||
                         match_ident(seq, "interface")) {
                    err = false;
                    if (!type_parse(stats, expr, seq, tmp, err, errc) || err) {
                        errc.say("expect type primitive type signature but not");
                        errc.trace(start, seq);
                        err = true;
                        return true;
                    }
                }
                else {
                    seq.rptr = begin;
                    return false;
                }
                auto res = std::make_shared<TypeNode>();
                res->str = typeprim_str_;
                res->type = std::static_pointer_cast<TypeNode>(tmp);
                res->pos = {start, seq.rptr};
                node = std::move(res);
                return true;
            };
        }

        constexpr auto null_with_type = [](auto&& stat_, auto&& type_, auto&& expr, auto& seq, auto& node, bool& err, auto& errc) { return false; };
    }  // namespace minilang
}  // namespace utils
