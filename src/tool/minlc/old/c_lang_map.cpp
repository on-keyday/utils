/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "c_lang.h"
#include "minlc.h"
#include <minilang/minlutil.h>
#include <minilang/minlstruct.h>
#include <random>

namespace minlc {
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

    // mut int -> int
    // int -> const int
    // *int -> int const* const
    // *mut int -> int*const
    // mut *int -> int*
    std::string minl_type_to_c_type(mi::MinLinkage linkage, const std::shared_ptr<mi::MinNode>& node, C& c) {
        auto typ = mi::is_Type(node);
        if (!typ) {
            return "void";
        }
        if (typ->str == mi::pointer_str_) {
            auto res = minl_type_to_c_type(linkage, typ->type, c);
            if (!res.size()) {
                return {};
            }
            return res + "*";
        }
        if (typ->str == mi::mut_str_) {
            return minl_type_to_c_type(linkage, typ->type, c);
        }
        if (typ->str == mi::const_str_) {
            auto res = minl_type_to_c_type(linkage, typ->type, c);
            if (!res.size()) {
                return {};
            }
            return res + " const";
        }
        if (typ->str == mi::va_arg_str_) {
            return "...";
        }
        if (auto func = mi::is_Func(typ)) {
            return minl_type_to_func_type(linkage, func, c);
        }
        if (auto param = mi::is_FuncParam(typ)) {
            return minl_type_to_c_type(linkage, typ->type, c);
        }
        return typ->str;
    }

    bool collect_funcarg(const std::shared_ptr<mi::FuncParamNode>& node, FuncParams& params, CFuncArgs& args, C& c) {
    }

    std::shared_ptr<CType> node_to_type(const std::shared_ptr<mi::TypeNode>& node, C& c, ErrC& errc) {}

    std::string minl_type_to_func_type(mi::MinLinkage linkage, const std::shared_ptr<mi::FuncNode>& node, C& c) {
        auto ret_type = minl_type_to_c_type(linkage, node->return_, c);
        if (ret_type.size() == 0) {
            return {};
        }
        std::vector<std::shared_ptr<mi::FuncParamNode>> params;
        mi::FuncParamToVec(params, node);
        std::vector<CFuncArg> args;
        std::string tydef_index = ret_type + "(*)(";
        for (auto& param : params) {
            CFuncArg arg;
            auto typ = minl_type_to_c_type(linkage, param->type, c);
            if (!typ.size()) {
                return {};
            }
            const auto need_comma = tydef_index.back() != '(';
            arg.type_ = typ;
            if (param->ident) {
                arg.argname = param->ident->str;
            }
            args.push_back(std::move(arg));
            if (need_comma) {
                tydef_index += ",";
            }
            tydef_index += typ;
        }
        tydef_index += ")";
        auto tydef = c.add_functype(node);
        tydef->return_ = ret_type;
        auto& ident = c.tydef_cache[tydef_index];
        if (ident.size() == 0) {
            ident = "Tmp_typedef_fn_" + gen_random(30);
        }
        tydef->identifier = ident;
        tydef->arguments = std::move(args);
        return tydef->identifier;
    }

    bool add_func_block(C& c, ErrC& errc, CFunction& func, mi::MinLinkage linkage) {
        if (func.identifier.size() == 0) {
            func.identifier = "anonymous_fn_" + gen_random(30);
        }
        if (!func.base->block) {
            return true;
        }
        auto block = mi::is_Block(func.base->block);
        auto func_scope = c.enter_scope(func.base);
        c.local->list.func_decs[func.identifier].push_back({
            func.base,
            linkage,
        });
        for (auto& arg : func.arguments) {
            c.local->list.variables[arg.argname].push_back({
                arg.base,
                linkage,
            });
        }
        func.block = minl_to_c_block(c, errc, block);
        return true;
    }

    bool vardef_expr(C& c, Statements& stat, const std::shared_ptr<mi::MinNode>& node) {
        auto bin = mi::is_Binary(node, true);
        if (mi::is_define_expr_and_ok(bin) <= 0) {
            return false;
        }
        std::vector<std::shared_ptr<mi::MinNode>> news, inits;
        if (!mi::BinaryToVec(news, bin->left) ||
            !mi::BinaryToVec(inits, bin->right)) {
            return false;
        }
        if (news.size() != inits.size()) {
            return false;
        }
        return true;
    }

    std::shared_ptr<CFunction> minl_to_c_local_function(C& c, ErrC& errc, std::shared_ptr<mi::FuncNode>& fn) {
        // local function captures variable used at it.
        // In order to support assignment to same function type
        // both captured and uncaptured function,
        // function would be wrapped with structure
    }

    std::shared_ptr<CExpr> minl_to_c_expr(C& c, ErrC& errc, const std::shared_ptr<mi::MinNode>& node) {
        if (!node) {
            return nullptr;
        }
        if (auto fn = mi::is_Func(node)) {
            auto func = c.add_function(errc, fn, mi::mlink_local);
            auto expr = std::make_shared<CExpr>();
            expr->node = node;
            expr->value = func->identifier;
            return expr;
        }
        if (auto bin = mi::is_Binary(node)) {
            if (bin->str == ":=" || bin->str == "=") {
                return nullptr;
            }
            auto expr = std::make_shared<CExpr>();
            expr->node = node;
            expr->value = bin->str;
            if (bin->left) {
                expr->left = minl_to_c_expr(c, errc, bin->left);
                if (!expr->left) {
                    return nullptr;
                }
            }
            expr->right = minl_to_c_expr(c, errc, bin->right);
            if (!expr->right) {
                return nullptr;
            }
            return expr;
        }
        auto expr = std::make_shared<CExpr>();
        expr->node = node;
        expr->value = node->str;
        return expr;
    }

    std::shared_ptr<CIfStat> minl_to_c_if(C& c, ErrC& errc, std::shared_ptr<mi::CondStatNode>& cond) {
        if (cond->str == mi::if_stat_str_[1]) {
            auto expr = minl_to_c_expr(c, errc, cond->left);
        }
        return nullptr;
    }

    std::shared_ptr<CBlock> minl_to_c_block(C& c, ErrC& errc, std::shared_ptr<mi::BlockNode>& block) {
        auto cur = block;
        auto block_scope = c.enter_scope(block);
        std::vector<std::shared_ptr<CStatement>> stat;
        while (cur) {
            auto target = cur->element;
            if (!target) {
                cur = cur->next;
                continue;
            }
            if (mi::is_Binary(target) || mi::is_Func(target) || target->node_type == mi::nt_min) {
                if (target->str == ":=") {
                    if (!vardef_expr(c, stat, target)) {
                        return nullptr;
                    }
                }
                else {
                    auto expr = minl_to_c_expr(c, errc, target);
                    if (!expr) {
                        return nullptr;
                    }
                    stat.push_back(expr);
                }
            }
            if (auto cond = mi::is_CondStat(target, true)) {
                auto stat = minl_to_c_if(c, errc, cond);
            }
            if (auto wst = mi::is_WordStat(target)) {
                if (wst->str == mi::return_str_) {
                    auto ret = std::make_shared<CReturnStat>();
                    if (wst->expr) {
                        ret->expr = minl_to_c_expr(c, errc, wst->expr);
                    }
                    stat.push_back(ret);
                }
            }
            cur = cur->next;
        }
        auto result = std::make_shared<CBlock>();
        result->statements = std::move(stat);
        result->symbols = c.local;
        return result;
    }
}  // namespace minlc
