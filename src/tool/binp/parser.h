/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <minilang/comb/branch_table.h>
#include <minilang/comb/token.h>
#include <minilang/comb/ident_num.h>
#include <minilang/comb/space.h>
#include <minilang/comb/comment.h>
#include <minilang/comb/string.h>
#include <minilang/comb/nullctx.h>

namespace binp {
    namespace parser {
        using namespace utils::minilang::comb;

        constexpr auto make_parser() {
            constexpr auto spaces_ = -group("spaces", ~(condfn("comment", known::c_comment) | condfn("space", ~known::consume_space) | condfn("line", known::consume_line)));
            constexpr auto spaces = COMB_PROXY(spaces);
            constexpr auto default_object = condfn("default_object", known::make_comment("(", ")", true));
            constexpr auto encode_method = condfn("encode_method", known::make_comment("[<", ">]", true));
            constexpr auto decode_method = condfn("decode_method", known::make_comment("[{", "}]", true));
            constexpr auto cpp_ident = known::c_ident & -~(cconsume("::") & +known::c_ident);
            constexpr auto attributes = spaces & -default_object & spaces & -encode_method & spaces & -decode_method;
            constexpr auto struct_r = COMB_PROXY(struct_);
            constexpr auto type = group("type", (struct_r | +condfn("ident", cpp_ident)) & attributes);
            constexpr auto field = group("field", condfn("ident", known::c_ident) & spaces & +type);
            constexpr auto struct_ = group("struct", ctoken("struct") & spaces & ctoken("{") & -~(spaces & CNot{"}"} & +field) & spaces & +ctoken("}"));
            constexpr auto type_r = COMB_PROXY(type);
            constexpr auto typedef_ = group("type", ctoken("type") & not_condfn(known::c_cont_ident) & spaces & condfn("type_name", known::c_ident) & spaces & +type_r);
            constexpr auto cpp_namespace = condfn("cpp_namespace", cpp_ident);
            constexpr auto namespace_ = group("namespace", ctoken("namespace") & not_condfn(known::c_cont_ident) & spaces & +cpp_namespace);
            constexpr auto include = group("include", ctoken("include") & not_condfn(known::c_cont_ident) & spaces & +COMB_PROXY(include_path));
            constexpr auto program = group("program", -~(spaces & include) & spaces & -namespace_ & -~(spaces & not_eos & +(include | typedef_)) & spaces & +eos);
            constexpr auto include_path = condfn("path", known::c_string | known::make_string_parser("<", ">"));

            struct {
                decltype(spaces_) spaces;
                decltype(struct_) struct_;
                decltype(type) type;
                decltype(include_path) include_path;
            } r{spaces_, struct_, type, include_path};
            return [program, r](auto&& seq, auto&& ctx) {
                return program(seq, ctx, r);
            };
        }

        constexpr auto parse = make_parser();

        constexpr bool check_parse() {
            auto seq = utils::make_ref_seq(R"(
                include <string>
                include "mylib.h"
                /*code generated*/
                namespace mockgen::sample
                include "dynamic.h"
                /*implements comment*/
                type Type struct {
                    test1 byte(0)[</*encode*/>][{/*deocde*/}]
                    test2 byte[<encode method>]
                    test3 uint32(default value)
                }(object::create())

                type Type2 Object(Object::create())[<encode>][{/*decode*/}]
            )");
            if (parse(seq, NullCtx{}) != CombStatus::match) {
                return false;
            }
            return true;
        }

        static_assert(check_parse());
    }  // namespace parser
}  // namespace binp
