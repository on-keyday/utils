/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "comb.h"
#include "token.h"

namespace utils::minilang::comb::known {
    constexpr auto space_token = ctoken(" ");
    constexpr auto tab_token = ctoken("\t");
    constexpr auto space_or_tab_token = (space_token | tab_token);
    constexpr auto line_token = (ctoken("\r\n") | ctoken("\n") | ctoken("\r"));
    constexpr auto space_or_line = space_or_tab_token | line_token;
    constexpr auto space_seq = ~space_token;
    constexpr auto space_or_tab_seq = ~space_or_tab_token;
    constexpr auto space_or_line_seq = ~space_or_line;

    constexpr auto consume_space = cconsume(" ");
    constexpr auto consume_tab = cconsume("\t");
    constexpr auto consume_space_or_tab = (consume_space | consume_tab);
    constexpr auto consume_line = (cconsume("\r\n") | cconsume("\n") | cconsume("\r"));
    constexpr auto consume_space_or_line = consume_space_or_tab | consume_line;
    constexpr auto consume_space_or_tab_seq = ~consume_space_or_tab;
    constexpr auto consume_space_or_line_seq = ~consume_space_or_line;

}  // namespace utils::minilang::comb::known
