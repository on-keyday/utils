/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <minilang/token/parsers.h>
#include "symbols.h"
#include "func.h"

namespace vstack {
    namespace token = utils::minilang::token;

    constexpr auto tokens() {
        auto symbols = token::yield_defined_symbols(
            token::one_of_kind(
                s::add, s::sub, s::mul, s::div, s::mod, s::equal, s::not_equal,
                s::l_and, s::l_or, s::braces_begin, s::braces_end,
                s::parentheses_begin, s::parentheses_end, s::comma, s::assign));
        auto ident = token::yield_ident();
        auto str = token::yield_string();
        auto num = token::yield_radix_number(false);
        auto block = token::yield_block_comment("/*", "*/");
        auto space = token::yield_space();
        auto line = token::yield_line();
        auto keyword = token::yield_defined_keywords(
            token::one_of_kind(k::return_, k::fn_, k::let_));
        auto err = token::yield_error("expect token but cannit tokenize");
        auto tokens = token::yield_oneof(space, line, block, str, symbols, keyword, num, ident, err);
        return token::not_eof_then(tokens);
    }

    constexpr auto expr() {
        auto skip = token::skip_spaces({});
        auto expect = token::symbol_expector();
        auto peek = token::symbol_expector(false);
        auto brackets = token::brackets(expect(s::parentheses_begin), token::ctx_expr(), skip(expect(s::parentheses_end)));
        auto prim = token::yield_oneof(token::adapt_expect(brackets), token::ctx_primitive());
        auto after = token::binary_after(skip(prim), skip(token::yield_oneof(token::adapt_after(brackets), token::binary_right(token::yield_oneof(expect(s::dot))))));
        auto mul = token::binary(after, skip(expect(s::mul, s::div, s::mod)));
        auto add = token::binary(mul, skip(expect(s::add, s::sub)));
        auto eq = token::binary(add, skip(expect(s::equal, s::not_equal)));
        auto and_ = token::binary(eq, skip(expect(s::l_and)));
        auto or_ = token::binary(and_, skip(expect(s::l_or)));
        auto comma = token::binary(or_, skip(expect(s::comma)));
        return comma;
    }

    constexpr auto statement() {
        auto skip = token::skip_spaces({});
        auto kexpect = token::keyword_expector();
        auto return_ = token::expect_then(token::ctx_expr(), skip(kexpect(k::return_)));
        auto let_ = skip(let_def(token::ctx_expr()));
        auto fn_ = symbol::block_scope(skip(fn_def(token::ctx_statement())));
        auto stat = token::yield_oneof(return_, let_, fn_, token::ctx_expr());
        return stat;
    }

    constexpr auto parse = token::yield_until_eof(token::ctx_statement());

    namespace symbol {

    }

}  // namespace vstack
