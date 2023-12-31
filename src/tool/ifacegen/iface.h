/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#include <comb2/composite/range.h>
#include <comb2/basic/group.h>
#include <comb2/basic/literal.h>
#include <comb2/basic/unicode.h>
#include <comb2/basic/peek.h>
#include <comb2/basic/logic.h>
#include <comb2/composite/string.h>

namespace ifacegen {
    constexpr auto k_token = "token";
    constexpr auto k_ident = "ident";
    constexpr auto k_file = "file";
    constexpr auto k_def = "def";
    constexpr auto k_comments = "comments";
    constexpr auto k_import = "import";
    constexpr auto k_macro = "macro";
    constexpr auto k_alias = "alias";
    constexpr auto k_typeparam = "typeparam";
    constexpr auto k_interface = "interface";
    constexpr auto k_func = "func";
    constexpr auto k_type = "type";
    constexpr auto k_pointer = "pointer";
    constexpr auto k_package = "package";
    namespace parser {
        constexpr auto src = R"def(
        ROOT:=PACKAGE? [TYPEPARAM? [INTERFACE|ALIAS]|MACRO|IMPORT]*? EOF
        PACKAGE:="package" ID!
        IMPORT:="import" UNTILEOL
        MACRO:= "macro" ID UNTILEOL
        ALIAS:="alias" ID UNTILEOL
        INTERFACE:= "interface"[ ID "{" FUNCDEF*? "}" ]! EOS
        TYPEPARAM:="typeparam" TYPENAME ["," TYPENAME]*?
        TYPENAME:="..." ID!|TYPEID DEFTYPE?
        TYPEID:="^"? ID 
        DEFTYPE:="=" ID
        FUNCDEF:="const"? "noexcept"? ID ["(" FUNCLIST? ")" TYPE ["=" ID!]? ]! EOS
        POINTER:="*"*
        FUNCLIST:=VARDEF ["," FUNCLIST! ]?
        VARDEF:=ID TYPE
        TYPE:="..."? ["&&"|"&"]? POINTER? "const"? ID
    )def";

        constexpr auto make_parser() {
            namespace cps = utils::comb2::composite;
            using namespace utils::comb2::ops;
            auto tok = [&](auto a) {
                return str(k_token, lit(a));
            };

            auto until_eol = +~(not_(cps::eol) & uany);
            auto sp = cps::space | cps::tab;
            auto ident = str(k_ident, cps::c_ident & -~("::"_l & +cps::c_ident));
            auto num_or_ident = str(k_ident, ~cps::c_ident_next);
            auto pkg = group(k_package, tok("package") & ~sp & +ident);
            auto skip_sp = -~sp;
            auto skip_sp_eol = -~(cps::space | cps::tab | cps::eol);
            auto comment = str("comment", tok("#") & until_eol);
            auto comments = group(k_comments, comment & -cps::eol & -~(skip_sp_eol & comment));
            auto import_ = group(k_import, tok("import") & ~sp & +str(k_file, until_eol));
            auto macro = group(k_macro, tok("macro") & ~sp & ident & str(k_def, until_eol));
            auto alias_impl = tok("alias") & ~sp & ident & str(k_def, until_eol);
            auto ptr = str(k_pointer, ~"*"_l);
            auto typ_impl = -(tok("...") & skip_sp) & -((tok("&&") | tok("&")) & skip_sp) & -ptr & skip_sp & -(tok("const") & ~sp) & +ident;
            auto type = group(k_type, typ_impl);
            auto var_def = ident & skip_sp & +type;
            auto func_param_impl = skip_sp & tok("(") & skip_sp & -(var_def & skip_sp & -~(tok(",") & skip_sp & var_def & skip_sp)) & +tok(")");
            auto func_sig_impl = func_param_impl & skip_sp & type & -(skip_sp & tok("=") & skip_sp & +num_or_ident);
            auto func_def_impl = -(tok("const") & ~sp) & -(tok("noexcept") & ~sp) & ident & func_sig_impl & skip_sp;
            auto func_def = group(k_func, func_def_impl);
            auto interface_ = tok("interface") & (sp | cps::eol) & skip_sp_eol & ident & skip_sp_eol & +tok("{") &
                              skip_sp_eol & -comments & -~(func_def & skip_sp_eol & -comments & skip_sp_eol) & +tok("}");
            auto type_name_template = tok("...") & skip_sp & ident;
            auto type_name_typeid = -(tok("^") & skip_sp) & ident;
            auto type_name_deftype = skip_sp & tok("=") & skip_sp & ident;
            auto type_name = type_name_template | type_name_typeid & -type_name_deftype;
            auto typeparam_impl = tok("typeparam") & ~sp & (type_name & skip_sp & -~(tok(",") & skip_sp & type_name & skip_sp));
            auto typeparam = group(k_typeparam, typeparam_impl);
            auto iface = group(k_interface, -typeparam & skip_sp_eol & interface_);
            auto alias = group(k_alias, -typeparam & skip_sp_eol & alias_impl);
            auto prog = skip_sp_eol & comments & skip_sp_eol & -(pkg & skip_sp_eol) &
                        -~((comments | iface | alias | macro | import_) & skip_sp_eol) & +eos;
            return [prog](auto&& seq, auto&& ctx) {
                return prog(seq, ctx, 0);
            };
        }

        constexpr auto parse = make_parser();
    }  // namespace parser
}  // namespace ifacegen
