/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "range.h"
#include "number.h"

namespace futils::comb2::composite {
    constexpr auto back_slash = lit('\\');
    constexpr auto hex2_str = hexadecimal_number & hexadecimal_number;

    constexpr auto hex4_str = hex2_str & hex2_str;

    constexpr auto oct3_str = octal_number & octal_number & octal_number;

    constexpr auto hex_str = lit('x') & hex2_str;
    constexpr auto oct_str = oct3_str;
    constexpr auto utf16_str = lit('u') & hex4_str;
    constexpr auto utf32_str = lit('U') & hex4_str & hex4_str;

    constexpr auto make_strlit(auto after_bs, auto normal) {
        return (back_slash &
                +(hex_str |
                  oct_str |
                  utf16_str |
                  utf32_str |
                  after_bs)) |
               normal;
    }

    constexpr auto strlit = make_strlit(eol | uany, uany);

    constexpr auto make_string(auto quote, auto end_cond, auto inner) {
        return quote & *(not_(end_cond) & inner) & +quote;
    }

    constexpr auto c_str = make_string(lit('"'), lit('"') | eol, strlit);
    constexpr auto char_str = make_string(lit('\''), lit('\'') | eol, strlit);
    constexpr auto js_regex_str = make_string(lit('/'), lit('/') | eol, strlit);
    constexpr auto go_raw_str = make_string(lit('`'), lit('`'), uany);
    constexpr auto py_doc_str_double = make_string(lit("\"\"\""), lit("\"\"\""), uany);
    constexpr auto py_doc_str_single = make_string(lit("'''"), lit("'''"), uany);

    constexpr auto inner_cpp_raw_str = proxy([](auto&& seq, auto&& ctx, auto&&) {
        const auto b = seq.rptr;
        while (seq.current() != '(') {
            if (seq.eos()) {
                return Status::not_match;
            }
            seq.consume();
        }
        const auto br = seq.rptr;
        auto match_end = [&] {
            if (seq.current() == ')') {
                auto tmpb = seq.rptr;
                seq.consume();
                for (auto i = 0; i < br - b; i++) {
                    if (seq.current() != seq.buf.buffer[b + i]) {
                        seq.rptr = tmpb;
                        return false;
                    }
                    seq.consume();
                }
                return true;
            }
            return false;
        };
        seq.consume();
        while (!match_end()) {
            if (seq.eos()) {
                return Status::not_match;
            }
            seq.consume();
        }
        return Status::match;
    });

    constexpr auto cpp_raw_str = 'R'_l & '"'_l & inner_cpp_raw_str & '"'_l;

    namespace test {
        constexpr auto check_string() {
            auto seq = make_ref_seq(R"("object\
                ")");
            return c_str(seq, comb2::test::TestContext{}, 0) == Status::match;
        }

        static_assert(check_string());
    }  // namespace test

}  // namespace futils::comb2::composite
