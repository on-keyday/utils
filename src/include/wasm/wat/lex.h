/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#include <comb2/basic/group.h>
#include <comb2/composite/range.h>
#include <comb2/basic/unicode.h>
#include <comb2/composite/comment.h>

namespace futils::wasm::wat::lex {
    using namespace comb2::ops;

    namespace cps = comb2::composite;

    constexpr auto line_comment = cps::comment(lit(";;"), any, cps::line_feed | cps::eos);

    constexpr auto block_comment = cps::comment(lit("(;"), any, +not_(cps::eos) & lit(";)"), true, [](auto&& seq, auto&& ctx, auto&& rec) {
        ctxs::context_error(seq, ctx, "unexpected EOF while parsing comment. expect ;)");
    });

    constexpr auto format = cps::tab | cps::line_feed | cps::carriage_return;
    constexpr auto space = cps::space | format;

    constexpr auto sign = lit('+') | lit('-');
    constexpr auto digit = cps::digit;
    constexpr auto hex_digit = cps::hex;

    constexpr auto num = digit & *(digit | '_' & +digit);
    constexpr auto hex_num = hex_digit & *(hex_digit | '_' & +hex_digit);

    constexpr auto uN = num | lit("0x") & +hex_num;
    constexpr auto sN = -sign & num;
    constexpr auto iN = uN | sN;
    constexpr auto frac = num;
    constexpr auto hex_frac = hex_num;

    constexpr auto float_ = num & -(lit('.') & -frac & -((lit('e') | lit('E')) & -sign & +num));

    constexpr auto hex_float = hex_num & -(lit('.') & -hex_frac & -((lit('p') | lit('P')) & -sign & +num));

    constexpr auto fNmag = float_ | hex_float | lit("inf") | lit("nan") | lit("nan:0x") & +hex_num;
    constexpr auto fN = sign & fNmag;

    constexpr auto string_char = lit("\\t") | lit("\\n") | lit("\\r") | lit("\\\"") | lit("\\\\") | lit("\\0") | lit("\\x") & hex_num;
}  // namespace futils::wasm::wat::lex
