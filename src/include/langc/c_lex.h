/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <comb2/composite/string.h>
#include <comb2/composite/indent.h>
#include <comb2/composite/comment.h>
#include <comb2/composite/string.h>
#include <comb2/basic/group.h>
#include <comb2/tree/branch_table.h>
#include <optional>

namespace futils {
    namespace langc {

        constexpr auto k_arithmetic = "arithmetric";
        constexpr auto k_compare = "compare";
        constexpr auto k_logical = "logical";
        constexpr auto k_bit = "bit";
        constexpr auto k_misc = "misc";
        constexpr auto k_assign = "assign";
        constexpr auto k_address = "address";
        constexpr auto k_sign = "sign";
        constexpr auto k_integer_ty = "integer_type";
        constexpr auto k_float_ty = "float_type";
        constexpr auto k_complex_ty = "complex_type";
        constexpr auto k_struct_ty = "struct_type";
        constexpr auto k_qualifier = "qualifier";
        constexpr auto k_storage = "storage";
        constexpr auto k_typedef = "typedef";
        constexpr auto k_void = "void";
        constexpr auto k_condition = "condition";
        constexpr auto k_loop = "loop";
        constexpr auto k_branch = "branch";
        constexpr auto k_semicolon = "semicolon";
        constexpr auto k_thread = "thread";
        constexpr auto k_algin = "align";
        constexpr auto k_inline = "inline";
        constexpr auto k_sizeof = "sizeof";
        constexpr auto k_assert = "assert";
        constexpr auto k_generic = "generic";
        constexpr auto k_func_annot = "func_annot";
        constexpr auto k_comment = "comment";
        constexpr auto k_ident = "ident";
        constexpr auto k_str_lit = "str_literal";
        constexpr auto k_char_lit = "char_literal";
        constexpr auto k_int_lit = "int_literal";
        constexpr auto k_float_lit = "float_literal";
        constexpr auto k_preproc = "preproc";
        constexpr auto k_space = "space";
        constexpr auto k_line = "line";
        constexpr auto k_escape = "escape";
        constexpr auto k_unspec = "unspec";
        constexpr auto k_three_dot = "three_dot";

        namespace lex {
            using namespace comb2::ops;
            namespace cps = comb2::composite;

            constexpr auto op(auto op, auto&&... args) {
                return str(op, (... | lit(args)));
            }

            constexpr auto key(auto op, auto&&... args) {
                return str(op, (... | lit(args)) & not_(cps::c_ident_next));
            }

            constexpr auto comment = str(k_comment, cps::c_comment | cps::cpp_comment);
            constexpr auto space = str(k_space, ~(cps::space | cps::tab));
            constexpr auto line = str(k_line, cps::eol);
            constexpr auto unspec = str(k_unspec, ~(not_(cps::eol | cps::tab | range(0x20, 0x7E)) & uany) | any);

            constexpr auto address = op(k_address, ".", "->");

            constexpr auto arithmetic = op(k_arithmetic, "++", "+=", "+", "--", "-=", "-", "*=", "*", "/=", "/", "%=", "%");
            constexpr auto compare = op(k_compare, "<=", "<", ">=", ">", "!=", "==");
            constexpr auto logical = op(k_logical, "!", "&&", "||");
            constexpr auto bit = op(k_bit, "<<=", "<<", ">>=", ">>", "~", "&=", "&", "|=", "|", "^=", "^");
            constexpr auto misc = op(k_misc, "(", ")", "[", "]", "{", "}", "?", ":", ",");
            constexpr auto assign = op(k_assign, "=");
            constexpr auto semicolon = op(k_semicolon, ";");
            constexpr auto preproc = op(k_preproc, "#");
            constexpr auto escape = op(k_escape, "\\");
            constexpr auto three_dot = op(k_three_dot, "...");
            constexpr auto make_ops() {
                return three_dot | address | arithmetic | compare | logical | bit | misc | assign | semicolon | preproc | escape;
            }

            constexpr auto ops = make_ops();

            constexpr auto integer = key(k_integer_ty, "_Bool", "char", "short", "int", "long");
            constexpr auto sign = key(
                k_sign,
                "signed", "unsigned");
            constexpr auto floating = key(k_float_ty, "float", "double");
            constexpr auto complex = key(k_complex_ty, "_Complex", "_Imaginary");
            constexpr auto struct_ = key(k_struct_ty, "struct", "union", "enum");
            constexpr auto qualifier = key(k_qualifier, "volatile", "const", "restrict");
            constexpr auto storage = key(k_storage, "auto", "extern", "static", "register");
            constexpr auto typedef_ = key(k_typedef, "typedef");
            constexpr auto void_ = key(k_void, "void");
            constexpr auto condition = key(k_condition, "if", "else", "switch", "case", "default");
            constexpr auto loop = key(k_loop, "for", "while", "do");
            constexpr auto branch = key(k_branch, "goto", "continue", "break", "return");
            constexpr auto thread = key(k_thread, "_Atomic", "_Thread_local");
            constexpr auto align = key(k_algin, "_Alignas", "_Alignof");
            constexpr auto inline_ = key(k_inline, "inline");
            constexpr auto sizeof_ = key(k_sizeof, "sizeof");
            constexpr auto assert = key(k_assert, "_Static_assert");
            constexpr auto generic = key(k_generic, "_Generic");
            constexpr auto func_annot = key(k_func_annot, "_Noreturn");

            constexpr auto make_keywords() {
                return integer | sign | floating | complex | struct_ | qualifier | storage |
                       typedef_ | void_ | condition | loop | branch | thread | align | inline_ |
                       sizeof_ | assert | generic | func_annot;
            }

            constexpr auto keywords = make_keywords();
            constexpr auto ident = str(k_ident, cps::c_ident);
            constexpr auto int_literal = str(k_int_lit, (cps::not_dec_float & (cps::dec_integer | cps::oct_c_integer)) | (cps::not_hex_float & cps::hex_integer));
            constexpr auto str_literal = str(k_str_lit, cps::c_str);
            constexpr auto char_literal = str(k_char_lit, cps::char_str);
            constexpr auto float_literal = str(k_float_lit, cps::dec_float | cps::hex_float);

            constexpr auto make_literals() {
                return int_literal | float_literal | str_literal | char_literal;
            }

            constexpr auto literals = make_literals();

            constexpr auto make_lexer() {
                return *(space | line | comment | ops | keywords | literals | ident | unspec) & eos;
            }

            constexpr auto lexer = make_lexer();
        }  // namespace lex

        namespace ast {
            enum class UnaryOp {
                logical_not = 0,
                bit_not,
                increment,
                decrement,
                plus_sign,
                minus_sign,
                deref,
                address,
                size,
                align,
                post_increment,
                post_decrement,
            };
            constexpr const char* unary_op[] = {"!", "~", "++", "--", "+", "-", "*", "&", "sizeof", "_Alignof", nullptr};
            enum class BinaryOp {
                mul,
                div,
                mod,
                add,
                sub,
                left_shift,
                right_shift,
                less,
                less_or_eq,
                grater,
                grater_or_eq,
                equal,
                not_equal,
                bit_and,
                bit_xor,
                bit_or,
                logical_and,
                logical_or,
                cond_op1,
                cond_op2,
                assign,
                add_assign,
                sub_assign,
                mul_assign,
                div_assign,
                mod_assign,
                left_shift_assign,
                right_shift_assign,
                bit_and_assign,
                bit_or_assign,
                bit_xor_assign,
                comma,
            };

            constexpr const char* bin_layer0[] = {"*", "/", "%", nullptr};
            constexpr const char* bin_layer1[] = {"+", "-", nullptr};
            constexpr const char* bin_layer2[] = {"<<", ">>", nullptr};
            constexpr const char* bin_layer3[] = {"<", "<=", ">", ">=", nullptr};
            constexpr const char* bin_layer4[] = {"==", "!=", nullptr};
            constexpr const char* bin_layer5[] = {"&", nullptr};
            constexpr const char* bin_layer6[] = {"^", nullptr};
            constexpr const char* bin_layer7[] = {"|", nullptr};
            constexpr const char* bin_layer8[] = {"&&", nullptr};
            constexpr const char* bin_layer9[] = {"||", nullptr};
            constexpr const char* bin_layer10[] = {"?", ":", nullptr};
            constexpr const char* bin_layer11[] = {"=", "+=", "-=", "*=",
                                                   "/=", "%=", "<<=", ">>=",
                                                   "&=", "|=", "^=",
                                                   nullptr};
            constexpr const char* bin_layer12[] = {",", nullptr};

            constexpr auto bin_cond_layer = 10;
            constexpr auto bin_assign_layer = 11;
            constexpr auto bin_comma_layer = 12;

            constexpr const char* const* bin_layers[] = {
                bin_layer0,
                bin_layer1,
                bin_layer2,
                bin_layer3,
                bin_layer4,
                bin_layer5,
                bin_layer6,
                bin_layer7,
                bin_layer8,
                bin_layer9,
                bin_layer10,
                bin_layer11,
                bin_layer12,
            };

            constexpr size_t bin_layer_len = sizeof(bin_layers) / sizeof(bin_layers[0]);

            constexpr std::optional<BinaryOp> bin_op(const char* l) {
                size_t i = 0;
                for (auto& layer : bin_layers) {
                    size_t j = 0;
                    while (layer[j]) {
                        if (layer[j] == l) {
                            return BinaryOp(i);
                        }
                        j++;
                        i++;
                    }
                }
                return std::nullopt;
            }

        }  // namespace ast

        using Pos = comb2::Pos;

        struct Token {
            std::string token;
            const char* tag = nullptr;
            Pos pos;
        };

        template <class T>
        std::optional<std::vector<Token>> tokenize(Sequencer<T>& s) {
            comb2::tree::BranchTable tbl;
            tbl.lexer_mode = true;
            if (lex::lexer(s, tbl, 0) != comb2::Status::match) {
                return std::nullopt;
            }
            std::vector<Token> tokens;
            comb2::tree::visit_nodes(tbl.root_branch, [&](auto& a, bool e) {
                if constexpr (std::is_same_v<decltype(a), comb2::tree::Ident<const char*>&>) {
                    tokens.push_back(Token{
                        .token = a.ident,
                        .tag = a.tag,
                        .pos = a.pos,
                    });
                }
            });
            return tokens;
        }

    }  // namespace langc
}  // namespace futils
