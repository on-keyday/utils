/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "token.h"
#include "../../number/parse.h"

namespace utils::minilang::comb::known {
    constexpr auto make_c_ident(bool first_num) {
        return [=](auto& seq, auto&&, auto&&) {
            auto is_ident = [&](auto c, bool inc_num) {
                return ('a' <= c && c <= 'z') ||
                       ('A' <= c && c <= 'Z') ||
                       c == '_' ||
                       (inc_num && '0' <= c && c <= '9');
            };
            if (!is_ident(seq.current(), first_num)) {
                return CombStatus::not_match;
            }
            seq.consume();
            while (is_ident(seq.current(), true)) {
                seq.consume();
            }
            return CombStatus::match;
        };
    };

    constexpr auto c_ident = make_c_ident(false);

    constexpr auto c_cont_ident = make_c_ident(true);

    constexpr auto make_integer_parser(int radix_base, int sep = 0) {
        return [=](auto& seq, auto&&, auto&&) {
            int radix = radix_base;
            if (radix == 0) {
                if (seq.seek_if("0x") || seq.seek_if("0X")) {
                    radix = 16;
                }
                else if (seq.seek_if("0o") || seq.seek_if("0O")) {
                    radix = 8;
                }
                else if (seq.seek_if("0b") || seq.seek_if("0B")) {
                    radix = 2;
                }
                else {
                    radix = 10;
                }
            }
            if (!number::is_radix_char(seq.current(), radix)) {
                return CombStatus::not_match;
            }
            seq.consume();
            while (number::is_radix_char(seq.current(), radix) ||
                   (sep != 0 && seq.current() == sep)) {
                seq.consume();
            }
            return CombStatus::match;
        };
    }

    // .0   // float
    // 0.e0 // float
    // 0e0  // float
    // 0+0  // int
    // 0e+0 // float
    constexpr auto make_float_parser(int radix_base, int dot = '.', int sep = 0, bool allow_int = false) {
        return [=](auto& seq, auto&& ctx, auto&&) {
            int radix = radix_base;
            bool must_int = false;
            if (radix == 0) {
                if (seq.seek_if("0x") || seq.seek_if("0X")) {
                    radix = 16;
                }
                else if (seq.seek_if("0o") || seq.seek_if("0O")) {
                    must_int = true;
                    radix = 8;
                }
                else if (seq.seek_if("0b") || seq.seek_if("0B")) {
                    must_int = true;
                    radix = 2;
                }
                else {
                    radix = 10;
                }
            }
            if (must_int && !allow_int) {
                return CombStatus::not_match;
            }
            auto match_exp = [&] {
                return radix == 10 && (seq.match("e") || seq.match("E")) ||
                       radix == 16 && (seq.match("p") || seq.match("P"));
            };
            auto is_ok = [&] {
                return number::is_radix_char(seq.current(), radix);
            };
            auto is_skip = [&] {
                return sep != 0 && seq.current() == sep;
            };
            auto prev_is_skip = [&] {
                return seq.current(-1) == sep;
            };
            bool has_num = false;

            // read number
            // 0.00e+2
            // ^
            if (seq.current() != dot) {
                if (!is_ok()) {
                    return CombStatus::not_match;
                }
                has_num = true;
                seq.consume();
                while (is_ok() || is_skip()) {
                    seq.consume();
                }
            }
            // detect integer
            const auto has_no_dot = seq.current() != dot;
            const auto has_no_exponent = (has_num && !match_exp());
            if (has_no_dot && has_no_exponent) {
                if (allow_int) {
                    return CombStatus::match;
                }
                return CombStatus::not_match;
            }
            if (must_int) {
                return CombStatus::not_match;
            }
            // read dot and decimals
            // 0.00e+2
            //  ^^^
            if (seq.current() == dot) {
                seq.consume();
                if (!has_num) {
                    if (!is_ok()) {
                        // only dot
                        // .
                        return CombStatus::not_match;
                    }
                }
                while (is_ok() || is_skip()) {
                    seq.consume();
                }
            }
            // read exponent
            // 0.00e+2
            //     ^^^
            if (match_exp()) {
                seq.consume();
                (void)(seq.consume_if('+') || seq.consume_if('-'));
                if (!number::is_digit(seq.current())) {
                    report_error(ctx, "expect digit number but not");
                    return CombStatus::fatal;
                }
                while (number::is_digit(seq.current()) || is_skip()) {
                    seq.consume();
                }
            }
            return CombStatus::match;
        };
    };

    constexpr auto integer = make_integer_parser(0);
    constexpr auto floating_point_number = make_float_parser(0);

    namespace test {
        constexpr bool check_float() {
            auto do_test = [](const char* t, CombStatus suc = CombStatus::match) {
                auto seq = make_ref_seq(t);
                auto res = floating_point_number(seq, 0, 0);
                if (res == suc) {
                    return true;
                }
                return false;
            };
            return do_test("0.0") &&
                   do_test("0.") &&
                   do_test(".0") &&
                   do_test(".", CombStatus::not_match) &&
                   do_test("0", CombStatus::not_match) &&
                   do_test("102302", CombStatus::not_match) &&
                   do_test("0x0.1") &&
                   do_test("0b0", CombStatus::not_match) &&
                   do_test("0.e0") &&
                   do_test("0e0") &&
                   do_test("0x0pf", CombStatus::fatal) &&
                   do_test("0.0e", CombStatus::fatal);
        }

        static_assert(check_float());
    }  // namespace test
}  // namespace utils::minilang::comb::known
