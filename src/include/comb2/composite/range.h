/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../basic/literal.h"
#include "../basic/logic.h"
#include "../basic/unicode.h"

namespace utils::comb2::composite {
    using namespace ops;
    constexpr auto small_alphabet = range('a', 'z');
    constexpr auto large_alphabet = range('A', 'Z');
    constexpr auto alphabet = small_alphabet | large_alphabet;
    constexpr auto digit_number = range('0', '9');
    constexpr auto c_ident_first = alphabet | lit('_');
    constexpr auto c_ident_next = c_ident_first | digit_number;

    constexpr auto c_ident = c_ident_first & -~c_ident_next;

    constexpr auto byte_order_mark = ulit(unicode::byte_order_mark);
    constexpr auto win_eol = '\r'_l & '\n'_l;
    constexpr auto unix_eol = '\n'_l;
    constexpr auto oldmac_eol = '\r'_l;

    constexpr auto line_feed = unix_eol;
    constexpr auto carriage_return = oldmac_eol;

    constexpr auto eol = win_eol | unix_eol | oldmac_eol;

    constexpr auto space = lit(' ');
    constexpr auto tab = lit('\t');

    namespace test {
        constexpr auto check_ident() {
            auto seq = make_ref_seq("ident");
            auto seq2 = make_ref_seq("_Vtable2");
            auto seq3 = Sequencer<const byte*>{unicode::utf8::utf8_bom, 3};
            return c_ident(seq, 0, 0) == Status::match &&
                   c_ident(seq2, 0, 0) == Status::match &&
                   byte_order_mark(seq3, 0, 0) == Status::match;
        }

        static_assert(check_ident());
    }  // namespace test
}  // namespace utils::comb2::composite
