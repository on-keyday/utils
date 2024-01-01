/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <comb2/composite/comment.h>
#include <comb2/composite/indent.h>
#include <comb2/basic/group.h>

namespace oslbgen {

    constexpr auto k_args = "args";
    constexpr auto k_define = "define";
    constexpr auto k_fn = "fn";
    constexpr auto k_ident = "ident";
    constexpr auto k_token = "token";
    constexpr auto k_strc = "strc";
    constexpr auto k_inner = "inner";
    constexpr auto k_param = "param";
    constexpr auto k_return = "return";
    constexpr auto k_spec = "spec";
    constexpr auto k_setspec = "setspec";
    constexpr auto k_use = "use";
    constexpr auto k_comment = "comment";
    constexpr auto k_preproc = "preproc";
    constexpr auto k_alias = "alias";

    constexpr auto gen_parser() {
        using namespace futils::comb2::ops;
        namespace cps = futils::comb2::composite;
        auto ident = str(k_ident, cps::c_ident);
        auto space = cps::space | cps::tab;
        auto space_sep = space & -~space;
        auto spaces = -~space;
        auto skip = -~(space | cps::eol);
        auto until_eol = +~(not_(cps::eol) & uany);
        auto args = group(k_args, lit("(") & spaces & -(ident & spaces & -~(lit(",") & spaces & ident)) & +lit(")"));
        auto define = group(k_define, lit("define") & space_sep & ident & -args & space_sep & str(k_token, until_eol));
        auto spec = group(k_spec, str("spec", lit("no") | lit("cc") | lit("bc") | lit("ls")) & -args);
        auto fn = group(k_fn, lit("fn") & space_sep & ident & spaces & +str(k_param, cps::comment(lit('('), +not_(cps::eol) & any, lit(')'), true)) & spaces & str(k_return, until_eol));
        auto strc = group(k_strc, lit("strc") & args & space_sep & ident & spaces & str(k_inner, cps::comment(lit('{'), any, lit('}'), true)));
        auto setspec = group(k_setspec, lit("spec") & space_sep & spec);
        auto use = group(k_use, lit("use") & space_sep & str(k_token, until_eol));
        auto alias = group(k_alias, lit("alias") & space_sep & str(k_token, ~(not_(space | cps::eol) & any)) & space_sep & str(k_token, until_eol));
        auto preproc = str(k_preproc, cps::shell_comment);
        auto comment = str(k_comment, cps::c_comment | cps::cpp_comment);
        auto prog = -~(skip & (comment | preproc | define | fn | strc | setspec | use | alias)) & skip & +eos;
        return [prog](auto&& seq, auto&& ctx) {
            return prog(seq, ctx, 0);
        };
    }

    constexpr auto parse = gen_parser();

    constexpr auto test_parser(auto&& ctx) {
        auto ptr = futils::make_ref_seq(R"(
        // this is test
        /*you can comment like this*/
        fn test_fn (write argument here) result type
        define Object(identifier) identifier
        define next(it) it->next
        define Value 1
        strc(enum) Enum {
            a,
            b,
            c
        }
        spec ls(openssl)
        spec no
        use STACK_OF(Type)
        alias STACK_OF(Type) STACK_OF(Type)
        #include <cstddef>
    )");
        auto res = parse(ptr, ctx);
        return res == futils::comb2::Status::match;
    }

    static_assert(test_parser(futils::comb2::test::TestContext{}));
}  // namespace oslbgen
