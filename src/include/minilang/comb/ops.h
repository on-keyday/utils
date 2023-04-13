/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "token.h"

namespace utils::minilang::comb::known {

    // operators

    // arithmetic operation
    constexpr auto add_sign = CToken{"+"};
    constexpr auto sub_sign = CToken{"-"};
    constexpr auto mul_sign = CToken{"*"};
    constexpr auto pow_sign = CToken{"**"};
    constexpr auto div_sign = CToken{"/"};
    constexpr auto int_div_sign = CToken{"//"};
    constexpr auto mod_sign = CToken{"%"};

    // comparison operation
    constexpr auto equal_sign = CToken{"=="};
    constexpr auto not_equal_sign = CToken{"!="};
    constexpr auto strict_equal_sign = CToken{"==="};
    constexpr auto strict_not_equal_sign = CToken{"!=="};
    constexpr auto grater_sign = CToken{">"};
    constexpr auto grater_or_equal_sign = CToken{">="};
    constexpr auto less_sign = CToken{"<"};
    constexpr auto less_or_equal_sign = CToken{"<="};
    constexpr auto three_way_compare_sign = CToken{"<=>"};

    // bit operation
    constexpr auto bit_right_shift_sign = CToken{">>"};
    constexpr auto bit_left_shift_sign = CToken{"<<"};
    constexpr auto bit_three_right_shift_sign = CToken{">>>"};
    constexpr auto bit_three_left_shift_sign = CToken{"<<<"};
    constexpr auto bit_and_sign = CToken{"&"};
    constexpr auto bit_or_sign = CToken{"|"};
    constexpr auto bit_xor_sign = CToken{"^"};
    constexpr auto bit_not_sign = CToken{"~"};

    // logical operation
    constexpr auto and_sign = CToken{"&&"};
    constexpr auto or_sign = CToken{"||"};
    constexpr auto not_sign = CToken{"!"};
    constexpr auto null_coalescing_sign = CToken{"??"};

    // assignment operation
    constexpr auto assign_sign = CToken{"="};

    // modification operation

    // with arithmetic
    constexpr auto add_assign_sign = CToken{"+="};
    constexpr auto increment_sign = CToken{"++"};
    constexpr auto sub_assign_sign = CToken{"-="};
    constexpr auto decrement_sign = CToken{"--"};
    constexpr auto mul_assign_sign = CToken{"*="};
    constexpr auto div_assign_sign = CToken{"/="};
    constexpr auto int_div_assign_sign = CToken{"//="};
    constexpr auto mod_assign_sign = CToken{"%="};

    // with bit
    constexpr auto bit_right_shift_assign_sign = CToken{">>="};
    constexpr auto bit_left_shift_assign_sign = CToken{"<<="};
    constexpr auto bit_three_right_shift_assign_sign = CToken{">>>="};
    constexpr auto bit_three_left_shift_assign_sign = CToken{"<<<="};
    constexpr auto bit_and_assign_sign = CToken{"&="};
    constexpr auto bit_or_assign_sign = CToken{"|="};
    constexpr auto bit_xor_assign_sign = CToken{"^="};

    // with logical
    constexpr auto null_coalescing_assign_sign = CToken{
        "?"
        "?="};

    // access operation
    constexpr auto direct_access_sign = CToken{"."};
    constexpr auto indirect_access_sign = CToken{"->"};
    constexpr auto optional_access_sign = CToken{"?."};
    constexpr auto channel_sign = CToken{"<-"};
    constexpr auto pipe_sign = CToken{"|>"};

    // comment
    constexpr auto sharp_sign = CToken{"#"};
    constexpr auto c_comment_begin = CToken{"/*"};
    constexpr auto c_comment_end = CToken{"*/"};

    // brackets
    constexpr auto comma_sign = CToken{","};
    constexpr auto praenthesis_begin = CToken{"("};
    constexpr auto praenthesis_end = CToken{")"};
    constexpr auto braces_begin = CToken{"{"};
    constexpr auto braces_end = CToken{"}"};
    constexpr auto brackets_begin = CToken{"["};
    constexpr auto brackets_end = CToken{"]"};

    // symbol group

    // +,-
    constexpr auto add_symbol = not_then(add_assign_sign, add_sign) |
                                not_then(sub_assign_sign, sub_sign);

    // |
    constexpr auto bit_or_symbol = not_then(or_sign, bit_or_assign_sign, bit_or_sign);

    // *,/,%
    constexpr auto mul_symbol = not_then(mul_assign_sign, mul_sign) |
                                not_then(div_assign_sign, div_sign) |
                                not_then(mod_assign_sign, mod_sign);

    // &
    constexpr auto bit_and_symbol = not_then(and_sign, bit_and_assign_sign, bit_and_sign);

    // ==,!=
    constexpr auto equal_symbol = equal_sign | not_equal_sign;

    // <=>,<=,>=,<,>
    constexpr auto compare_symbol =
        three_way_compare_sign |
        less_or_equal_sign |
        grater_or_equal_sign |
        less_sign |
        grater_sign;
}  // namespace utils::minilang::comb::known
