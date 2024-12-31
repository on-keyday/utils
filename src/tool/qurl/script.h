/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <comb2/basic/literal.h>
#include <comb2/basic/group.h>
#include <comb2/composite/range.h>
#include <comb2/composite/number.h>

namespace qurl {
    namespace script {

        constexpr auto k_token = "token";
        constexpr auto k_ident = "ident";
        constexpr auto k_int = "int";
        constexpr auto g_assign = "assign";
        constexpr auto g_call = "call";
        constexpr auto g_expr = "expr";
        constexpr auto g_func = "fn";
        constexpr auto g_block = "block";
        constexpr auto g_return = "return";
        constexpr auto g_if = "if";

        constexpr auto get_parser() {
            using namespace futils::comb2::ops;
            namespace cps = futils::comb2::composite;
            auto tok = [&](auto val) {
                return str(k_token, val);
            };
            auto space_ = -~(cps::space | cps::tab | cps::eol);
            auto space = method_proxy(space);
            auto ident_ = str(k_ident, cps::c_ident);
            auto integer = str(k_int, cps::dec_integer | cps::hex_prefix & +cps::hex_integer);
            auto ident = method_proxy(ident);
            auto prim = method_proxy(prim);
            auto expr = method_proxy(expr);
            auto stat = method_proxy(stat);
            auto fn = method_proxy(fn);
            auto block = method_proxy(block);
            auto params = ident & -~(space & ','_l & space & +ident) & space & -(tok("..."_l) & space);
            auto fn_ = group(g_func, "fn"_l & not_(cps::c_ident_next) & space & +'('_l & space & -params & +')'_l & space & +block);
            auto brac = '('_l & space & +expr & space & +')'_l;
            auto prim_ = (brac | fn | integer | ident);
            auto call = group(g_call, '('_l & space & -(expr & space & -~(','_l & +expr & space)) & +')'_l);
            auto afters_ = group(g_expr, prim & -~(space & (call)));
            auto mul_ = group(g_expr, afters_ & -~(space & tok('*'_l | '/'_l | '%'_l) & space & +afters_));
            auto add_ = group(g_expr, mul_ & -~(space & tok('+'_l | '-'_l) & space & +mul_));
            auto cmp_ = group(g_expr, add_ & -~(space & tok("=="_l) & space & +add_));
            auto expr_ = cmp_;
            auto assign = group(g_assign, ident & space & (tok(-':'_l & '='_l)) & space & +expr);
            auto ret = group(g_return, tok("return"_l) & not_(cps::c_ident_next) & space & (';'_l | +expr));
            auto cond_ = +expr;
            auto elif_ = space & tok("elif"_l) & not_(cps::c_ident_next) & space & cond_ & space & +block;
            auto else_ = space & tok("else"_l) & not_(cps::c_ident_next) & space & +block;
            auto if_ = group(g_if, tok("if"_l) & not_(cps::c_ident_next) & space & cond_ & space & +block & -~elif_ & -else_);
            auto stat_ = if_ | ret | assign | expr;
            auto stats = -~(space & stat);
            auto block_ = group(g_block, '{'_l & stats & space & +'}'_l);
            auto prog = stats & space & +eos;
            struct {
                decltype(ident_) ident;
                decltype(prim_) prim;
                decltype(space_) space;
                decltype(expr_) expr;
                decltype(stat_) stat;
                decltype(fn_) fn;
                decltype(block_) block;
            } l{ident_, prim_, space_, expr_, stat_, fn_, block_};
            return [l, prog](auto&& seq, auto&& ctx) {
                return prog(seq, ctx, l);
            };
        }

        constexpr auto parse = get_parser();

    }  // namespace script
}  // namespace qurl
