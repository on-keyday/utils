/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "middle.h"
#include "parser.h"
#include <random>
#include <helper/splits.h>

namespace minlc {

    namespace util {
        constexpr auto ident_ = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789";

        std::string gen_random(size_t len = 20) {
            std::random_device dev;
            std::uniform_int_distribution gen(size_t(0), utils::strlen(ident_));
            std::string str;
            for (size_t i = 0; i < len; i++) {
                str.push_back(ident_[gen(dev)]);
            }
            return str;
        }

        sptr<mi::TypeNode> gen_type(std::string_view type) {
            auto str = std::string("mod mock type Mock ") + type.data();
            auto tes = parse(utils::make_ref_seq(str), mi::null_errc);
            if (!tes.second) {
                return nullptr;
            }
            auto block = mi::is_Block(tes.second);
            auto def = mi::is_TypeDef(block->element);
            return def->next->type;
        }
    }  // namespace util

    namespace middle {
        sptr<Expr> minl_to_expr(M& m, const sptr<mi::MinNode>& node, bool callee) {
            if (auto ty = mi::is_Type(node, true)) {
                auto texpr = make_middle<TypeExpr>(m, ty);
                texpr->callee = callee;
                return texpr;
            }
            if (auto bin = mi::is_Binary(node)) {
                if (bin->str == ":=") {
                    m.errc.say("unexpected expression. `:=` must appear at most once per expression");
                    m.errc.node(bin);
                    return nullptr;
                }
                if (bin->str == "->") {
                    m.errc.say("unexpected expresion. `->` is reserved symbol but not available now");
                    m.errc.node(bin);
                    return nullptr;
                }
                if (bin->str == ".") {
                    auto dot = make_middle<DotExpr>(m, node);
                    dot->callee = callee;
                    if (!mi::BinaryToVec(dot->dots, bin, ".")) {
                        m.errc.say("unexpected node. library bug!!");
                        m.errc.node(bin);
                        return nullptr;
                    }
                    for (auto& d : dot->dots) {
                        if (!mi::is_Ident(d)) {
                            m.errc.say("unexpected primtivie value. expect identifier");
                            m.errc.node(d);
                            return nullptr;
                        }
                    }
                    dot->lookedup = m.lookup_ident(dot->dots[0]->str);
                    dot->current = m.current;
                    return dot;
                }
                if (bin->str == "()") {
                    if (bin->left) {
                        auto call = make_middle<CallExpr>(m, bin);
                        call->callee = minl_to_expr(m, bin->left, true);
                        if (!call->callee) {
                            return nullptr;
                        }
                        if (bin->right) {
                            std::vector<sptr<mi::MinNode>> nodes;
                            if (!mi::BinaryToVec(nodes, bin->right, ",")) {
                                m.errc.say("unexpected node. library bug!!");
                                m.errc.node(bin->right);
                                return nullptr;
                            }
                            for (auto& c : nodes) {
                                auto expr = minl_to_expr(m, c);
                                if (!expr) {
                                    return nullptr;
                                }
                                call->const_eval = call->const_eval && expr->const_eval;
                                call->arguments.push_back(std::move(expr));
                            }
                        }
                        return call;
                    }
                    else {
                        auto par = make_middle<ParExpr>(m, bin);
                        par->target = minl_to_expr(m, bin->right, callee);
                        if (!par->target) {
                            return nullptr;
                        }
                        par->const_eval = par->target->const_eval;
                        return par;
                    }
                }
                auto expr = make_middle<BinaryExpr>(m, bin);
                bool const_eval = true;
                if (bin->left) {
                    expr->left = minl_to_expr(m, bin->left);
                    if (!expr->left) {
                        return nullptr;
                    }
                    const_eval = expr->left->const_eval;
                }
                if (bin->right) {
                    expr->right = minl_to_expr(m, bin->right);
                    if (!expr->right) {
                        return nullptr;
                    }
                    const_eval = const_eval && expr->right->const_eval;
                }
                expr->const_eval = const_eval;
                return expr;
            }
            if (auto fn = mi::is_Func(node)) {
                auto func = minl_to_func(m, fn);
                if (!func) {
                    return nullptr;
                }
                auto ret = make_middle<FuncExpr>(m, fn);
                ret->func = func;
                return ret;
            }
            if (mi::is_Ident(node)) {
                auto ident = make_middle<IdentExpr>(m, node);
                ident->current = m.current;
                ident->lookedup = m.lookup_ident(node->str);
                ident->callee = callee;
                return ident;
            }
            auto expr = make_middle<Expr>(m, node);
            expr->const_eval = true;
            if (auto num = mi::is_Number(node); num) {
                if (num->is_float) {
                    m.errc.say("floating number is now not available");
                    m.errc.node(num);
                    return nullptr;
                }
                if (!num->parsable) {
                    m.errc.say("number is too large to parse as 64 bit integer. wait for support of big integer");
                    m.errc.node(num);
                    return nullptr;
                }
                expr->type = m.types.get_integer_builtin(m, num->integer);
            }
            return expr;
        }

        sptr<Func> minl_to_func(M& m, const sptr<mi::FuncNode>& node) {
            auto fn = make_middle<Func>(m, node);
            mi::FuncParamToVec(fn->args, node);
            fn->type = std::static_pointer_cast<types::FunctionType>(types::to_type(m, node, true));
            if (!fn->type) {
                return nullptr;
            }
            {
                auto func_scope = m.enter_scope(node);
                m.add_function(node);  // for recursive call
                for (auto& arg : fn->args) {
                    if (arg.node->ident) {
                        for (auto&& ident :
                             utils::helper::split(arg.node->ident->str, ",")) {
                            m.add_variable(ident, arg.node);
                        }
                    }
                }
                if (node->block) {
                    auto block_scope = m.enter_scope(node->block);
                    auto block = mi::is_Block(node->block);
                    fn->block = minl_to_block(m, block);
                    if (!fn->block) {
                        return nullptr;
                    }
                }
            }
            m.add_function(node);  // add to parent scope
            return fn;
        }

        sptr<DefineExpr> minl_to_define_expr(M& m, const sptr<mi::MinNode>& node) {
            auto def = mi::is_Binary(node);
            if (!def) {
                m.errc.say("unexpected Node. library bug!!");
                m.errc.node(node);
                return nullptr;
            }
            auto expr = make_middle<DefineExpr>(m, def);
            std::vector<sptr<mi::MinNode>> exprs;
            mi::BinaryToVec(expr->define.ident, def->left);
            mi::BinaryToVec(exprs, def->right);
            for (auto& exp : exprs) {
                auto c = minl_to_expr(m, exp);
                if (!c) {
                    return nullptr;
                }
                expr->define.expr.push_back(std::move(c));
            }
            for (auto& id : expr->define.ident) {
                if (!mi::is_Ident(id)) {
                    m.errc.say("unexpected expression. left of `:=` must be identifier");
                    m.errc.node(id);
                    return nullptr;
                }
                m.add_variable(id->str, def);
            }
            return expr;
        }

        bool minl_to_expr_or_define(M& m, const sptr<mi::MinNode>& elm, sptr<Expr>& expr, sptr<DefineExpr>& def) {
            if (elm->str == ":=") {
                def = minl_to_define_expr(m, elm);
                if (!def) {
                    return false;
                }
            }
            else {
                expr = minl_to_expr(m, elm);
                if (!expr) {
                    return false;
                }
            }
            return true;
        }

        sptr<If> minl_to_if(M& m, const sptr<mi::IfStatNode>& elm) {
            auto if_ = make_middle<If>(m, elm);
            auto err = [&] {
                m.errc.say("at if statement");
                m.errc.node(elm);
                return nullptr;
            };
            if (mi::if_stat_str_[1] == elm->str) {
                if_->expr = minl_to_expr(m, elm->left);
                if (!if_->expr) {
                    return err();
                }
            }
            else if (mi::if_stat_str_[2] == elm->str) {
                if (!minl_to_expr_or_define(m, elm->left, if_->iniexpr, if_->inidef)) {
                    return err();
                }
                if_->expr = minl_to_expr(m, elm->right);
                if (!if_->expr) {
                    return err();
                }
            }
            else {
                m.errc.say("unexpected node");
                m.errc.node(elm);
                return nullptr;
            }
            auto block = mi::is_Block(elm->block);
            if_->block = minl_to_block(m, block);
            if (!if_->block) {
                return err();
            }
            if (elm->els) {
                if (auto els = mi::is_IfStat(elm->els)) {
                    if_->else_if = minl_to_if(m, els);
                    if (!if_->else_if) {
                        return err();
                    }
                }
                else {
                    auto block = mi::is_Block(elm->els, true);
                    if_->els = minl_to_block(m, block);
                    if (!if_->els) {
                        return err();
                    }
                }
            }
            return if_;
        }

        sptr<For> minl_to_for(M& m, const sptr<mi::ForStatNode>& elm) {
            auto for_ = make_middle<For>(m, elm);
            if (mi::for_stat_str_[0] == elm->str) {
                // nothing to do
            }
            else if (mi::for_stat_str_[1] == elm->str) {
                if (!elm->left) {
                    m.errc.say("expect expression but not found.library bug!!");
                    m.errc.node(elm);
                    return nullptr;
                }
            }
            m.errc.say("not implemented now");
            m.errc.node(elm);
            return nullptr;
        }

        bool minl_to_object(M& m, const sptr<mi::MinNode>& elm, std::vector<sptr<Object>>& vec) {
            if (elm->str == ":=") {
                auto def = minl_to_define_expr(m, elm);
                if (!def) {
                    return false;
                }
                vec.push_back(std::move(def));
            }
            else if (mi::is_Expr(elm)) {
                auto expr = minl_to_expr(m, elm);
                if (!expr) {
                    return false;
                }
                vec.push_back(std::move(expr));
            }
            else if (auto fn = mi::is_Func(elm)) {
                auto func = minl_to_func(m, fn);
                if (!func) {
                    return false;
                }
                vec.push_back(std::move(func));
            }
            else if (auto block = mi::is_Block(elm, true)) {
                auto elm = minl_to_block(m, block);
                if (!elm) {
                    return false;
                }
                vec.push_back(std::move(elm));
            }
            else if (auto wb = mi::is_WordStat(elm)) {
                if (wb->str == mi::return_str_) {
                    auto ret = make_middle<Return>(m, wb);
                    if (wb->expr) {
                        ret->expr = minl_to_expr(m, wb->expr);
                    }
                    vec.push_back(std::move(ret));
                }
                else if (wb->str == mi::nilstat_str_) {
                    // nothing to do
                }
                else if (wb->str == mi::linkage_str_) {
                    // nothing to do
                }
                else {
                    m.errc.say("unexpected node. library bug!!");
                    m.errc.node(elm);
                    return false;
                }
            }
            else if (auto com = mi::is_Comment(elm)) {
                vec.push_back(make_middle<Comment>(m, com));
            }
            else if (auto if_ = mi::is_IfStat(elm)) {
                auto expr = minl_to_if(m, if_);
                if (!expr) {
                    return false;
                }
                vec.push_back(std::move(expr));
            }
            else {
                m.errc.say("unexpected node. library bug!!");
                m.errc.node(elm);
                return false;
            }
            return true;
        }

        bool minl_to_objects(M& m, const sptr<mi::BlockNode>& node, std::vector<sptr<Object>>& vec) {
            auto cur = node->next;
            while (cur) {
                if (!cur->element) {
                    continue;
                }
                auto elm = cur->element;
                if (!minl_to_object(m, elm, vec)) {
                    return false;
                }
                cur = cur->next;
            }
            return true;
        }

        sptr<Block> minl_to_block(M& m, const sptr<mi::BlockNode>& node) {
            auto b = make_middle<Block>(m, node);
            if (!minl_to_objects(m, node, b->objects)) {
                return nullptr;
            }
            return b;
        }

        sptr<Program> minl_to_program(M& m, const sptr<mi::MinNode>& node) {
            auto block = mi::is_Block(node);
            if (!block) {
                m.errc.say("expect BlockNode but not");
                m.errc.node(node);
                return nullptr;
            }
            m = {};
            m.global = std::make_shared<mi::TopLevelSymbols>();
            if (!mi::collect_toplevel_symbol(m.global, node, m.errc)) {
                return nullptr;
            }
            auto prog = make_middle<Program>(m, block);
            for (auto& def : m.global->merged.func_defs) {
                for (auto& fn : def.second) {
                    auto f = minl_to_func(m, fn.node);
                    if (!f) {
                        return nullptr;
                    }
                    prog->global_funcs.push_back(f);
                }
            }
            if (!resolver::resolve(m)) {
                return nullptr;
            }
            return prog;
        }

    }  // namespace middle
}  // namespace minlc
