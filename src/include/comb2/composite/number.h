/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../basic/literal.h"
#include "../basic/logic.h"
#include "../basic/proxy.h"
#include "../basic/peek.h"
#include "../../number/radix.h"

namespace utils::comb2::composite {

    using namespace ops;

    constexpr auto radix_number(std::uint8_t radix) {
        return proxy([=](auto&& seq, auto&& ctx, auto&& rec) {
            auto c = seq.current();
            std::uint8_t cmp;
            if constexpr (sizeof(c) == 1) {
                cmp = std::uint8_t(c);
            }
            else {
                if (c < 0 || c > 0xff) {
                    return Status::not_match;
                }
                cmp = std::uint8_t(c);
            }
            if (!number::is_radix_char(cmp, radix)) {
                return Status::not_match;
            }
            seq.consume();
            return Status::match;
        });
    }
    constexpr auto binary_number = radix_number(2);
    constexpr auto octal_number = radix_number(8);
    constexpr auto decimal_number = radix_number(10);
    constexpr auto hexadecimal_number = radix_number(16);

    constexpr auto hex_prefix = '0'_l & ('x'_l | 'X'_l);
    constexpr auto oct_prefix = '0'_l & ('o'_l | 'O'_l);
    constexpr auto bin_prefix = '0'_l & ('b'_l | 'B'_l);

    constexpr auto hex_integer = hex_prefix & +~hexadecimal_number;
    constexpr auto oct_integer = oct_prefix & +~octal_number;
    constexpr auto bin_integer = bin_prefix & +~binary_number;
    constexpr auto dec_integer = ~decimal_number;

    constexpr auto make_dec_float(auto dot) {
        return (lit(dot) & ~decimal_number |
                ~decimal_number & -(lit(dot) & -~decimal_number)) &
               -(('e'_l | 'E'_l) & -('+'_l | '-'_l) & +~decimal_number);
    }

    constexpr auto dec_float = make_dec_float('.');
    constexpr auto not_dec_float = not_('.'_l | ~decimal_number & ('.'_l | 'e'_l | 'E'_l));

    constexpr auto make_hex_float(auto dot) {
        return hex_prefix & +(((lit(dot) & ~hexadecimal_number) |
                               (~hexadecimal_number & -(lit(dot) & -~hexadecimal_number))) &
                              -(('p'_l | 'P'_l) & -('+'_l | '-'_l) & +~decimal_number));
    }
    constexpr auto hex_float = make_hex_float('.');
    constexpr auto not_hex_float = not_(hex_prefix & ('.'_l | ~hexadecimal_number & ('.'_l | 'p'_l | 'P'_l)));

    namespace test {
        constexpr auto check_number() {
            auto test = [](auto& parse, auto str, Status s = Status::match) {
                auto seq = make_ref_seq(str);
                if (parse(seq, 0, 0) != s) {
                    comb2::test::error_if_constexpr();
                }
            };
            test(hex_float, "0x.01p20");
            test(dec_float, "0.012");
            test(dec_float, ".02E+2");
            test(not_dec_float, "12030");
            test(not_dec_float, ".12030", Status::not_match);
            test(not_hex_float, "0x0200");
            test(hex_float, "0x0pf", Status::fatal);
            return true;
        }
        static_assert(check_number());
    }  // namespace test

}  // namespace utils::comb2::composite
