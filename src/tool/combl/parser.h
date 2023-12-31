/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <minilang/comb/print.h>
#include <minilang/comb/ident_num.h>
#include <minilang/comb/ops.h>
#include <minilang/comb/space.h>
#include <minilang/comb/make_json.h>
#include <minilang/comb/comment.h>
#include <minilang/comb/string.h>
#include <minilang/comb/indent.h>
#include <minilang/comb/unicode.h>

namespace combl::parser {
    using namespace utils::minilang::comb;
    constexpr auto spaces = COMB_PROXY(spaces);

    constexpr auto get_expr() {
        auto expr_r = COMB_PROXY(expr);
        auto fn_expr = COMB_PROXY(fn_expr);
        auto string_r = COMB_PROXY(string);
        auto if_ = COMB_PROXY(if_);
        auto for_ = COMB_PROXY(for_);
        auto switch_ = COMB_PROXY(switch_);
        auto type = COMB_PROXY(type);
        auto parenthesis = group("praenthesis", ctoken("(") & +expr_r & +ctoken(")"));
        auto primitive = -spaces & group("primitive", parenthesis | string_r | condfn("integer", known::integer) | fn_expr | if_ | for_ | switch_ | +condfn("ident", known::c_ident));
        auto call_op = group("call", ctoken("(") & (-spaces & ctoken(")") | +expr_r & +ctoken(")")));
        auto colon_or_expr = (cpeek(":") | +expr_r);
        auto end_or_expr = (cpeek("]") | +expr_r);
        auto index_op = group("index", ctoken("[") & -spaces & colon_or_expr & -spaces & -(ctoken(":") & end_or_expr) & -spaces & +ctoken("]"));
        auto peek_type_assert = peekfn(cconsume(".") & -known::consume_space_or_line_seq & cconsume("<"));
        auto type_assert = (peek_type_assert & group("type_assert", ctoken(".") & -spaces & ctoken("<") & -spaces & +type & -spaces & +ctoken(">")));
        auto access = group("access", ctoken(".") & -spaces & +condfn("member", known::c_ident));
        auto incr = group("increment", ctoken("++"));
        auto decr = group("decrement", ctoken("--"));
        auto postfix_do = -~(-spaces & (call_op | index_op | type_assert | access | incr | decr));
        auto postfix = group("postfix", primitive & postfix_do);
        auto mul = group("mul", COMB_PROXY(postfix) & -~(-spaces & known::mul_symbol & +COMB_PROXY(postfix)));
        auto add = group("add", mul & -~(-spaces & known::add_symbol & +mul));
        auto compare = group("compare", add & -~(-spaces & known::compare_symbol & +add));
        auto equal = group("equal", compare & -~(-spaces & known::equal_symbol & +compare));
        auto comma = group("comma", equal & -~(-spaces & known::comma_sign & +equal));
        auto assign = group("assign", comma & -~(-spaces & known::assign_sign & COMB_PROXY(assign)));
        auto expr = group("expr", COMB_PROXY(assign));
        auto no_comma_expr = group("expr", add);
        auto string = condfn("string", known::c_string) | condfn("char", known::js_string) | condfn("raw_string", known::go_multi_string);
        struct {
            decltype(assign) assign;                // expression entry
            decltype(expr) expr;                    // expression
            decltype(no_comma_expr) no_comma_expr;  // for argument init
            decltype(string) string;
            decltype(postfix) postfix;
        } r{assign, expr, no_comma_expr, string, postfix};
        return r;
    }

    constexpr auto get_types() {
        auto fn_type_r = COMB_PROXY(fn_type);
        auto type_r = COMB_PROXY(type);
        auto type_annon_r = COMB_PROXY(type_annon);
        auto peek_package = peekfn(known::c_ident & -known::consume_space_or_line_seq & cconsume("."));
        auto package_ = peek_package & +condfn("package_name", known::c_ident) & -spaces & +ctoken(".") & -spaces;
        auto ident = -package_ & condfn("type_ident", known::c_ident);
        auto struct_field = group("field", condfn("name", known::c_ident) & type_annon_r);
        auto struct_ = group("struct", ctoken("struct") & not_condfn(known::c_cont_ident));
        auto type_annon = group("type_annon", -spaces & ctoken(":") & type_r);
        auto type = group("type", -spaces & +(fn_type_r | ctoken("*") & type_r | ident));
        struct {
            decltype(type) type;
            decltype(type_annon) type_annon;
        } r{type, type_annon};
        return r;
    }

    constexpr auto get_funcs() {
        auto type = COMB_PROXY(type);
        auto block = COMB_PROXY(block);
        auto expr = COMB_PROXY(no_comma_expr);
        auto type_annon = COMB_PROXY(type_annon);
        auto argument = group("arg", type_annon | condfn("arg_ident", known::c_ident) & -spaces & -type_annon);
        auto arguments = group("args", -spaces & argument & -spaces & -~(ctoken(",") & -spaces & +argument & -spaces));
        auto arguments_braces = -spaces & ctoken("(") & -arguments & +ctoken(")");
        auto return_type = group("return_type", -spaces & ctoken("->") & type);
        auto fn_type = group("fn_type", ctoken("fn") & -spaces & cpeek("(") & +arguments_braces & -return_type);
        auto decl_or_block = -spaces & (cpeek(";") | +block);
        auto fn_def =
            group("fn_def", ctoken("fn") & spaces & condfn("fn_name", known::c_ident) & +arguments_braces & -return_type & decl_or_block);
        auto fn_expr = group("fn_expr", ctoken("fn") & -spaces & arguments_braces & -return_type & +block);
        auto init = group("init", -spaces & ctoken("=") & +expr);
        auto let_vars = condfn("ident", known::c_ident) & (type_annon & -init | +init);
        auto let = group("let", -spaces & ctoken("let") & spaces & +let_vars);
        struct {
            decltype(fn_def) fn_def;
            decltype(fn_type) fn_type;
            decltype(fn_expr) fn_expr;
            decltype(let) let;
        } r{fn_def, fn_type, fn_expr, let};
        return r;
    }

    constexpr auto get_control() {
        auto expr = COMB_PROXY(expr);
        auto block = COMB_PROXY(block);
        auto let = COMB_PROXY(let);
        auto let_or_expr = (peekfn(cconsume("let") & not_condfn(known::c_cont_ident)) & +let) | +expr;
        auto if_expr = let_or_expr & -(-spaces & ctoken(";") & +expr);
        auto if_part = ctoken("if") & not_condfn(known::c_cont_ident) & -spaces & if_expr & -spaces & +block;
        auto peek_if = cconsume("if") & not_condfn(known::c_cont_ident);
        auto peek_else_if = peekfn(cconsume("else") & not_condfn(known::c_cont_ident) & known::consume_space_or_line_seq & peek_if);
        auto else_if_ = -spaces & peek_else_if & +ctoken("else") & +not_condfn(known::c_cont_ident) & -spaces & +if_part;
        auto else_ = -spaces & ctoken("else") & not_condfn(known::c_cont_ident) & -spaces & +block;
        auto if_ = group("if", if_part & -~else_if_ & -else_);
        // for ;; {}
        // for expr;expr{}
        // for expr {}
        // for let a=expr;a;{}
        // for ;;step {}
        auto for_end = cpeek("{");
        auto for_delim = ctoken(";");
        auto for_peek_delim = cpeek(";");
        auto for_fifth = +expr & -spaces & for_end;
        auto for_fourth = +for_delim & -spaces & (for_end | for_fifth);
        auto for_third = (for_peek_delim | +expr) & -spaces & (for_end | for_fourth);
        auto for_second = +for_delim & -spaces & (for_end | for_third);
        auto for_first = for_end | (for_peek_delim | let_or_expr) & -spaces & (for_end | for_second);
        auto for_ = group("for", ctoken("for") & not_condfn(known::c_cont_ident) & -spaces & for_first & +block);
        auto switch_ = group("switch", ctoken("switch") & not_condfn(known::c_cont_ident) & -spaces & let_or_expr & -spaces & +block);
        struct {
            decltype(if_) if_;
            decltype(for_) for_;
            decltype(switch_) switch_;
        } r{if_, for_, switch_};
        return r;
    }

    constexpr auto get_block() {
        auto fn_def = COMB_PROXY(fn_def);
        auto expr = COMB_PROXY(expr);
        auto block_r = COMB_PROXY(block);
        auto let = COMB_PROXY(let);
        auto delim_or_expr = -spaces & (cpeek(";") | cpeek("}") | +expr);
        auto return_ = group("return", -spaces & ctoken("return") & not_condfn(known::c_cont_ident) & delim_or_expr);
        auto break_ = group("break", -spaces & ctoken("break") & not_condfn(known::c_cont_ident) & delim_or_expr);
        auto continue_ = group("continue", -spaces & ctoken("continue") & not_condfn(known::c_cont_ident) & delim_or_expr);
        auto label_ = group("label", -spaces & known::c_ident & ctoken(":"));
        auto type_ = COMB_PROXY(type);
        auto case_type = ctoken("<") & -spaces & +type_ & -spaces & +ctoken(">");
        auto expr_or_type = -spaces & (case_type | +expr);
        auto case_ = group("case", -spaces & ctoken("case") & not_condfn(known::c_cont_ident) & expr_or_type & -spaces & +ctoken(":"));
        auto default_ = group("default", -spaces & ctoken("default") & -spaces & ctoken(":"));
        auto elements = (fn_def | return_ | break_ | continue_ | let | block_r | ctoken(";") | case_ | default_ | label_ | +expr);
        auto inner_block = -~(-spaces & CNot{"}"} & +not_eos & elements);
        auto block = group("block", -spaces & ctoken("{") & +(inner_block & -spaces & +ctoken("}")));
        auto program = group("program", -~(-spaces & not_eos & elements) & -spaces & eos);
        struct {
            decltype(block) block;
            decltype(program) program;
        } r{block, program};
        return r;
    }

    constexpr auto make_parser() {
        auto exprs = get_expr();
        auto types = get_types();
        auto funcs = get_funcs();
        auto blocks = get_block();
        auto controls = get_control();
        auto program = blocks.program;
        auto comment = condfn("line_comment", known::cpp_comment) | condfn("block_comment", known::nest_c_comment);
        auto spaces = group("spaces", ~(known::space_or_line | comment));

        struct {
            decltype(exprs.assign) assign;
            decltype(exprs.expr) expr;
            decltype(exprs.no_comma_expr) no_comma_expr;
            decltype(exprs.string) string;
            decltype(exprs.postfix) postfix;
            decltype(types.type) type;
            decltype(types.type_annon) type_annon;
            decltype(funcs.fn_def) fn_def;
            decltype(funcs.fn_type) fn_type;
            decltype(funcs.fn_expr) fn_expr;
            decltype(funcs.let) let;
            decltype(blocks.block) block;
            decltype(spaces) spaces;
            decltype(controls.if_) if_;
            decltype(controls.for_) for_;
            decltype(controls.switch_) switch_;
        } r{exprs.assign, exprs.expr, exprs.no_comma_expr, exprs.string, exprs.postfix,
            types.type, types.type_annon, funcs.fn_def, funcs.fn_type, funcs.fn_expr, funcs.let,
            blocks.block, spaces,
            controls.if_, controls.for_, controls.switch_};

        return [program, r](auto&& seq, auto&& ctx) {
            return program(seq, ctx, r);
        };
    }

    constexpr auto parse = make_parser();
    constexpr bool comptime_check(auto&& ctx) {
        auto seq = utils::make_ref_seq(R"(
            /*/*ok bokujo*/*/
            // compiletime parser check
            fnc()
            fn main(argc :int,argv :**char) {
                0 + 2;
                x=x=x+1
                fn(){}+ident
                main(fn(){})
                "escape\\from"
                `go
                literal`[0:2][0:][:2][0]
                return;
                return 0
                if 32+4 {
                    return 0;
                }
                else {

                }
                if false {

                }
                else if true {

                } else {}
                for ;;{}
                for{}
                for true {}
                for let i =0;i<10; {}
                for a;b;c{
                    break;
                    continue;
                    break 1
                    continue 2
                }
                switch a {
                case 1:
                case <*int>:
                default:
                }
                if a {} else1
                a.b.c;
                a.b.<*int>;
                a++;
                a--;
                a++++;
            }
        )");
        if (parse(seq, ctx) != CombStatus::match) {
            return false;
        }
        return true;
    }

    static_assert(comptime_check(CounterCtx{}));

}  // namespace combl::parser
