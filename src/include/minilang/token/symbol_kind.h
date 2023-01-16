/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// symbol_kind - symbol kind
#pragma once
#include "sort_internal.h"

namespace utils {
    namespace minilang::token {
        enum class SymbolKind {
            add,                       // +
            sub,                       // -
            mul,                       // *
            div,                       // /
            mod,                       // %
            int_div,                   // //
            three_slash,               // ///
            comment_begin,             // /*
            comment_end,               // */
            b_and,                     // &
            b_or,                      // |
            b_xor,                     // ^
            b_not,                     // ~
            l_and,                     // &&
            l_or,                      // ||
            l_not,                     // !
            left_shift,                // <<
            right_shift,               // >>
            three_left_shift,          // <<<
            three_right_shift,         // >>>
            incr,                      // ++
            decr,                      // --
            equal,                     // ==
            type_equal,                // ===
            not_equal,                 // !=
            type_not_equal,            // !==
            greater,                   // >
            smaller,                   // <
            greater_equal,             // >=
            lambda_def,                // =>
            smaller_equal,             // <=
            dot,                       // .
            arrow,                     // ->
            chan_arrow,                // <-
            assign,                    // =
            add_assign,                // +=
            sub_assign,                // -=
            mul_assign,                // *=
            div_assign,                // /=
            mod_assign,                // %=
            int_div_assign,            // //=
            b_and_assign,              // &=
            b_or_assign,               // |=
            l_and_assign,              // &&=
            l_or_asssign,              // ||=
            three_left_shift_assign,   // <<<=
            three_right_shift_assign,  // >>>=
            left_shift_assign,         // <<=
            right_shift_assign,        // >>=
            pipe,                      // |>
            two_dot,                   // ..
            three_dot,                 // ...
            braces_begin,              // {
            braces_end,                // }
            parentheses_begin,         // (
            parentheses_end,           // )
            attribute_begin,           // [[
            attribute_end,             // ]]
            square_brackets_begin,     // [
            square_brackets_end,       // ]
            number_sign,               // #
            dollar_sign,               // $
            at_mark,                   // @
            at_mark_assign,            // @=
            null_coalescing,           // ??
            question,                  // ?
            back_slash,                /* \ */
            semi_colon,                // ;
            colon,                     // :
            comma,                     // ,
            string_lit,                // "
            char_lit,                  // '
            raw_string_lit,            // `
            three_raw_string_lit,      // ```
            define,                    // :=
            two_define,                // ::=
            end_of_mapping,
        };

        constexpr const char* symbol_mapping[int(SymbolKind::end_of_mapping)] = {
            "+",     // +
            "-",     // -
            "*",     // *
            "/",     // /
            "%",     // %
            "//",    // //
            "///",   // ///
            "/*",    // /*
            "*/",    // */
            "&",     // &
            "|",     // |
            "^",     // ^
            "~",     // ~
            "&&",    // &&
            "||",    // ||
            "!",     // !
            "<<",    // <<
            ">>",    // >>
            "<<<",   // <<<
            ">>>",   // >>>
            "++",    // ++
            "--",    // --
            "==",    // ==
            "===",   // ===
            "!=",    // !=
            "!==",   // !==
            ">",     // >
            "<",     // <
            ">=",    // >=
            "=>",    // =>
            "<=",    // <=
            ".",     // .
            "->",    // ->
            "<-",    // <-
            "=",     // =
            "+=",    // +=
            "-=",    // -=
            "*=",    // *=
            "/=",    // /=
            "%=",    // %=
            "//=",   // //=
            "&=",    // &=
            "|=",    // |=
            "&&=",   // &&=
            "||=",   // ||=
            "<<<=",  // <<<=
            ">>>=",  // >>>=
            "<<=",   // <<=
            ">>=",   // >>=
            "|>",    // |>
            "..",    // ..
            "...",   // ...
            "{",     // {
            "}",     // }
            "(",     // (
            ")",     // )
            "[[",    // [[
            "]]",    // ]]
            "[",     // [
            "]",     // ]
            "#",     // #
            "$",     // $
            "@",     // @
            "@=",    // @=
            "??",    // ??
            "?",     // ?
            "\\",    /* \ */
            ";",     // ;
            ":",     // :
            ",",     // ,
            "\"",    // "
            "'",     // '
            "`",     // `
            "```",   // ```
            ":=",    // :=
            "::=",   // :==
        };

        static_assert(internal::check_length(symbol_mapping), "mapping failed");

        constexpr auto apply_sorted_symbols(auto&& cb) {
            return internal::apply<SymbolKind, int(SymbolKind::end_of_mapping)>(cb, symbol_mapping);
        }

        constexpr auto symbols_tuple = apply_sorted_symbols([](auto&&... p) {
            return std::make_tuple(p...);
        });

    }  // namespace minilang::token
}  // namespace utils
