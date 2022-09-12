/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// minlfunc - minilang function declaeration/definition
#pragma once
#include "minl.h"
#include "minlstruct.h"

namespace utils {
    namespace minilang {
        constexpr auto func_def_str_ = "(function_define)";
        constexpr auto func_dec_str_ = "(function_declaere)";

        constexpr auto func_param_str_ = "(function_param)";

        constexpr auto func_return_param_str_ = "(function_named_return)";

        enum func_expr_mode {
            fe_def,
            fe_tydec,
            fe_expr,
            fe_iface,
        };

        // Derive TypeNode
        struct FuncParamNode : TypeNode {
            std::shared_ptr<FuncParamNode> next;
            std::shared_ptr<MinNode> ident;
            MINL_Constexpr FuncParamNode()
                : TypeNode(nt_funcparam) {}

           protected:
            MINL_Constexpr FuncParamNode(int id)
                : TypeNode(id) {}
        };

        // Derive FuncParamNode -> TypeNode
        struct FuncNode : FuncParamNode {
            std::shared_ptr<MinNode> return_;
            std::shared_ptr<MinNode> block;
            func_expr_mode mode{};
            MINL_Constexpr FuncNode()
                : FuncParamNode(nt_func) {}
        };

        bool func_parse_impl(func_expr_mode mode, const auto start,
                             auto&& type_, auto&& stat_, auto&& expr, auto& seq,
                             std::shared_ptr<MinNode>& node, bool& err, auto& errc) {
            auto error = [&](auto... msg) {
                errc.say(msg...);
                errc.trace(start, seq);
                err = true;
                return true;
            };
            auto read_params = [&](std::shared_ptr<FuncParamNode>& par, bool is_retrun, std::shared_ptr<FuncParamNode>* root) {
                auto single = "";
                auto expect = "";
                if (is_retrun) {
                    single = func_return_param_str_;
                    expect = "function return parameter";
                }
                else {
                    single = func_param_str_;
                    expect = "function parameter";
                }
                err = false;
                if (helper::space::consume_space(seq, true); seq.seek_if(")")) {
                    goto END_OF_PARAM;
                }
                while (true) {
                    std::string param_name;
                    bool no_param_name = false;
                    const auto param_start = seq.rptr;
                    const char* type_str = single;
                    auto param_end = seq.rptr;
                    if (!read_parameter(seq, param_end, param_name, no_param_name)) {
                        return error("expect ", expect, " name identifier but not");
                    }
                    std::shared_ptr<MinNode> tytmp;
                    if (!type_(stat_, expr, seq, tytmp, err, errc) || err) {
                        return error("expect ", expect, " type but not");
                    }
                    auto tmp = std::make_shared<FuncParamNode>();
                    tmp->str = type_str;
                    tmp->pos = {param_start, seq.rptr};
                    if (!no_param_name) {
                        tmp->ident = make_ident_node(param_name, {param_start, param_end});
                    }
                    tmp->type = std::static_pointer_cast<TypeNode>(tytmp);
                    if (par) {
                        par->next = tmp;
                    }
                    else {
                        if (root) {
                            *root = tmp;
                        }
                    }
                    par = tmp;
                    if (!seq.seek_if(",")) {
                        if (seq.seek_if(")")) {
                            break;
                        }
                        return error("expect ", expect, " , or ) but not");
                    }
                    helper::space::consume_space(seq, true);
                }
            END_OF_PARAM:
                return true;
            };
            helper::space::consume_space(seq, true);
            std::string name;
            const auto ident_start = seq.rptr;
            auto ident_end = seq.rptr;
            if (mode == fe_def || mode == fe_iface) {
                if (!ident_default_read(name, seq)) {
                    seq.rptr = start;
                    return false;
                }
                ident_end = seq.rptr;
                helper::space::consume_space(seq, true);
            }
            if (!seq.seek_if("(")) {
                return error("expect function parameter begin ( but not");
            }
            auto fnroot = std::make_shared<FuncNode>();
            const char* tys_ = func_dec_str_;
            std::shared_ptr<FuncParamNode> par = fnroot;
            err = false;
            read_params(par, false, nullptr);
            if (err) {
                return true;
            }
            helper::space::consume_space(seq, false);
            if (!(seq.eos() || helper::match_eol<false>(seq) || seq.match("{"))) {
                std::shared_ptr<MinNode> tmp;
                if (seq.seek_if("(")) {
                    std::shared_ptr<FuncParamNode> ret, root;
                    read_params(ret, true, &root);
                    if (err) {
                        return true;
                    }
                    tmp = std::move(root);
                }
                else {
                    if (!type_(stat_, expr, seq, tmp, err, errc) || err) {
                        return error("expect function return type but not");
                    }
                }
                fnroot->return_ = std::move(tmp);
                helper::space::consume_space(seq, false);
            }
            const auto no_block = mode == fe_tydec || mode == fe_iface;
            if (!no_block && seq.match("{")) {
                err = false;
                std::shared_ptr<MinNode> stat_node;
                if (!stat_(stat_, expr, seq, stat_node, err, errc) || err) {
                    return error("expect function statement block but not");
                }
                fnroot->block = std::move(stat_node);
                tys_ = func_def_str_;
            }
            else {
                if (mode == fe_expr) {
                    return error("expect function statement begin { but not");
                }
            }
            if (mode == fe_def) {
                fnroot->ident = make_ident_node(name, {ident_start, ident_end});
            }
            fnroot->str = tys_;
            fnroot->pos = {start, seq.rptr};
            fnroot->mode = mode;
            node = std::move(fnroot);
            return true;
        }

        constexpr auto func_expr(func_expr_mode mode) {
            return [=](auto&& type_, auto&& stat_, auto&& expr, auto& seq, std::shared_ptr<MinNode>& node, bool& err, auto& errc) -> bool {
                MINL_FUNC_LOG("func_expr")
                helper::space::consume_space(seq, true);
                const auto start = seq.rptr;
                if (!expect_ident(seq, "fn")) {
                    return false;
                }
                return func_parse_impl(mode, start, type_, stat_, expr, seq, node, err, errc);
            };
        }

        constexpr auto let_def_str_ = "(let_def)";
        constexpr auto let_def_group_str_ = "(let_def_group)";
        constexpr auto const_def_str_ = "(const_def)";
        constexpr auto const_def_group_str_ = "(const_def_group)";

        struct LetNode : MinNode {
            std::shared_ptr<MinNode> ident;
            std::shared_ptr<TypeNode> type;
            std::shared_ptr<MinNode> init;
            std::shared_ptr<LetNode> next;
            MINL_Constexpr LetNode()
                : MinNode(nt_let) {}
        };

        constexpr auto let_stat() {
            return [](auto&& type_, auto&& stat, auto&& expr, auto& seq, std::shared_ptr<MinNode>& node, bool& err, auto& errc) -> bool {
                MINL_FUNC_LOG("let_stat")
                helper::space::consume_space(seq, true);
                const auto start = seq.rptr;
                const char* typ_str;
                const char* grp_str;
                const char* expect;
                if (expect_ident(seq, "let")) {
                    typ_str = let_def_str_;
                    grp_str = let_def_group_str_;
                    expect = "variable";
                }
                else if (expect_ident(seq, "const")) {
                    typ_str = const_def_str_;
                    grp_str = const_def_group_str_;
                    expect = "constant value";
                }
                else {
                    return false;
                }
                helper::space::consume_space(seq, true);
                std::shared_ptr<LetNode> root, letnode;
                auto read_param_type = [&] {
                    const auto param_start = seq.rptr;
                    auto param_end = param_start;
                    std::string str;
                    while (true) {
                        if (!ident_default_read(str, seq)) {
                            errc.say("expected ", expect, " name identifier but not");
                            errc.trace(start, seq);
                            return true;
                        }
                        param_end = seq.rptr;
                        helper::space::consume_space(seq, false);
                        if (seq.seek_if(",")) {
                            helper::space::consume_space(seq, true);
                            str.push_back(',');
                            continue;
                        }
                        break;
                    }
                    std::shared_ptr<MinNode> typ, init;
                    if (!seq.match("=") && !seq.eos() && !helper::match_eol<false>(seq)) {
                        if (!type_(stat, expr, seq, typ, err, errc) || err) {
                            errc.say("expect ", expect, " type but not");
                            errc.trace(start, seq);
                            err = true;
                            return true;
                        }
                        helper::space::consume_space(seq, false);
                    }
                    if (seq.seek_if("=")) {
                        init = expr(seq);
                        if (!init) {
                            errc.say("expect ", expect, " initialization expression but not");
                            errc.trace(start, seq);
                            err = true;
                            return true;
                        }
                    }
                    auto lnode = std::make_shared<LetNode>();
                    lnode->str = typ_str;
                    lnode->ident = make_ident_node(str, {param_start, param_end});
                    lnode->type = std::static_pointer_cast<TypeNode>(typ);
                    lnode->init = std::move(init);
                    letnode->next = lnode;
                    letnode = lnode;
                    return true;
                };
                err = false;
                const auto group_start = seq.rptr;
                root = std::make_shared<LetNode>();
                root->str = grp_str;
                if (seq.seek_if("(")) {
                    while (true) {
                        helper::space::consume_space(seq, true);
                        if (seq.seek_if(")")) {
                            break;
                        }
                        read_param_type();
                        if (err) {
                            return true;
                        }
                    }
                }
                else {
                    read_param_type();
                }
                if (err) {
                    return true;
                }
                root->pos = {start, seq.rptr};
                node = std::move(root);
                return true;
            };
        }

        constexpr auto statment(auto... stats) {
            auto f = [=](auto&& stat, auto&& expr, auto& seq, std::shared_ptr<MinNode>& node, bool& err, auto& errc) -> bool {
                auto fold = [&](auto&& fn) -> bool {
                    return fn(stat, expr, seq, node, err, errc);
                };
                return (... || fold(stats));
            };
            return [=](auto&& stat, auto&& expr, auto& seq, std::shared_ptr<MinNode>& node, bool& err, auto& errc) -> bool {
                MINL_FUNC_LOG("statement")
                return f(stat, expr, seq, node, err, errc);
            };
        }

        constexpr auto wrap_with_type(auto&& with_type, auto&& type_sig) {
            return [=](auto&& stat, auto&& expr, auto& seq, std::shared_ptr<MinNode>& node, bool& err, auto& errc) {
                MINL_FUNC_LOG("wrap_with_type")
                return with_type(type_sig, stat, expr, seq, node, err, errc);
            };
        }

        constexpr auto make_func(func_expr_mode mode, auto&& type_sig) {
            return wrap_with_type(func_expr(mode), type_sig);
        }

        constexpr auto make_funcexpr_primitive(auto&& stats, auto&& sig) {
            auto fexpr = make_func(fe_expr, sig);
            return [=](auto&& expr, auto& seq, std::shared_ptr<MinNode>& node, bool& err, auto& errc) {
                MINL_FUNC_LOG("funcexpr_primitive")
                return fexpr(stats, expr, seq, node, err, errc);
            };
        }

        constexpr auto primitives(auto... prims) {
            return [=](auto&& expr, auto& seq, std::shared_ptr<MinNode>& node, bool& err, auto& errc) {
                auto fold = [&](auto&& prim) {
                    return prim(expr, seq, node, err, errc);
                };
                return (... || fold(prims));
            };
        }

        constexpr auto make_stats(auto&& prim, auto&& stat) {
            return make_exprs_with_statement(prim, stat);
        }

        constexpr auto typesig_default = type_signatures(struct_signature(), func_expr(fe_tydec));
        constexpr auto typedef_default = type_define(typesig_default);

    }  // namespace minilang
}  // namespace utils
