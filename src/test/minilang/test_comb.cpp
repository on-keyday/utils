/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <minilang/comb/token.h>
#include <minilang/comb/branch_table.h>
#include <minilang/comb/print.h>
#include <wrap/cout.h>
#include <minilang/comb/space.h>
#include <minilang/comb/ident_num.h>
#include <minilang/comb/ops.h>

using namespace utils::minilang::comb;
auto& cout = utils::wrap::cout_wrap();
constexpr auto spaces = group("spaces", known::space_seq);
constexpr bool test_expr(auto&& ctx) {
    constexpr auto expr = COMB_PROXY(expr);
    constexpr auto parenthesis = group("praenthesis", ctoken("(") & +expr & +ctoken(")"));
    constexpr auto primitive = -spaces & (parenthesis | condfn("integer", known::integer) | +condfn("ident", known::c_ident));
    constexpr auto mul = primitive & -~(-spaces & known::mul_symbol & +primitive);
    constexpr auto add = mul & -~(-spaces & known::add_symbol & +mul);
    constexpr auto assign = add & -~(-spaces & known::assign_sign & COMB_PROXY(assign));
    constexpr auto parse = group("expr", assign);
    struct {
        decltype(assign)& expr;
        decltype(assign)& assign;
    } rec{assign, assign};
    auto seq = utils::make_ref_seq("x=(23+53)*3/2");
    if (parse(seq, ctx, rec) != CombStatus::match) {
        return false;
    }
    return true;
}

constexpr bool test_fndef(auto&& ctx) {
    constexpr auto fndef = COMB_PROXY(fn);
    constexpr auto type = -spaces & +(fndef | condfn("type_ident", known::c_ident));
    constexpr auto type_annon = -group("type_annon", -spaces & ctoken(":") & type);
    constexpr auto argument = group("arg", condfn("arg_ident", known::c_ident) & -spaces & type_annon);
    constexpr auto arguments = group("args", -spaces & -(argument & -spaces & -~(ctoken(",") & -spaces & +argument & -spaces)));
    constexpr auto arguments_braces = -spaces & ctoken("(") & arguments & +ctoken(")");
    constexpr auto return_type = -group("return_type", -spaces & ctoken("->") & type);
    constexpr auto fn_type = group("fn_type", ctoken("fn") & -spaces & cpeek("(") & +arguments_braces & return_type);
    constexpr auto parse =
        group("fn", ctoken("fn") & spaces & +(condfn("fn_name", known::c_ident) & +arguments_braces & return_type));
    auto seq = utils::make_ref_seq("fn main(argc,argv,completion :fn()->bool)->fnc");
    struct {
        decltype(fn_type)& fn;
    } r{fn_type};
    if (parse(seq, ctx, r) != CombStatus::match) {
        return false;
    }
    return true;
}

int main() {
    static_assert(test_expr(CounterCtx{}));
    static_assert(test_fndef(CounterCtx{}));
    PrintCtx<decltype(cout)> p{cout};
    p.set.branch = false;
    p.set.group = false;
    p.br.discard_not_match = true;
    test_fndef(p);
    namespace cb = utils::minilang::comb;
    namespace br = utils::minilang::comb::branch;
    auto for_each = [](const std::shared_ptr<br::Element>& elm, br::CallEntry ent) {
        if (ent == br::CallEntry::leave) {
            return br::CallNext::skip;
        }
        if (auto el = br::is_Branch(elm); el && el->status == cb::CombStatus::not_match) {
            return br::CallNext::skip;
        }
        if (auto tok = br::is_Token(elm)) {
            cout << tok->token;
        }
        else if (auto id = br::is_Ident<const char*>(elm)) {
            cout << id->ident;
        }
        return br::CallNext::cont;
    };
    p.br.iterate(for_each);
    cout << "\n";
    p.br.root_branch = nullptr;
    p.br.current_branch = nullptr;
    test_expr(p);
    p.br.iterate(for_each);
    cout << "\n";
}
