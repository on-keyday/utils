/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// prefix - number with prefix
#pragma once

#include "../helper/strutil.h"
#include "../helper/appender.h"
#include "parse.h"

namespace utils {
    namespace number {
        template <class String>
        constexpr int has_prefix(String&& str) {
            if (helper::starts_with(str, "0x") || helper::starts_with(str, "0X")) {
                return 16;
            }
            else if (helper::starts_with(str, "0o") || helper::starts_with(str, "0O")) {
                return 8;
            }
            else if (helper::starts_with(str, "0b") || helper::starts_with(str, "0B")) {
                return 2;
            }
            return 0;
        }

        template <class T>
        constexpr int has_prefix(Sequencer<T>& seq) {
            if (seq.match("0x") || seq.match("0X")) {
                return 16;
            }
            else if (seq.match("0o") || seq.match("0O")) {
                return 8;
            }
            else if (seq.match("0b") || seq.match("0B")) {
                return 2;
            }
            return 0;
        }

        template <class T, class Int>
        constexpr NumErr prefix_integer(Sequencer<T>& seq, Int& res, int* pradix = nullptr) {
            int radix = 10;
            bool minus = false;
            if (seq.consume_if('+')) {
            }
            else if (std::is_signed_v<Int> && seq.consume_if('-')) {
                minus = true;
            }
            if (auto v = has_prefix(seq)) {
                radix = v;
                seq.consume(2);
                if (seq.current() == '+' || seq.current() == '-') {
                    return NumError::invalid;
                }
            }
            auto err = number::parse_integer(seq, res, radix);
            if (!err) {
                return err;
            }
            if (minus) {
                res = -res;
            }
            if (pradix) {
                *pradix = radix;
            }
            return true;
        }

        template <class T, class Int>
        constexpr NumErr prefix_integer(T&& input, Int& res, int* pradix = nullptr, size_t offset = 0, bool expect_eof = true) {
            auto seq = make_ref_seq(input);
            seq.rptr = offset;
            if (auto err = prefix_integer(seq, res, pradix); !err) {
                return err;
            }
            if (expect_eof && !seq.eos()) {
                return NumError::not_eof;
            }
            return true;
        }

        template <class Out>
        constexpr bool append_prefix(Out& out, int radix, bool upper = false) {
            if (radix == 16) {
                helper::append(out, upper ? "0X" : "0x");
            }
            else if (radix == 2) {
                helper::append(out, upper ? "0B" : "0b");
            }
            else if (radix == 8) {
                helper::append(out, upper ? "0O" : "0o");
            }
            else {
                return false;
            }
            return true;
        }
    }  // namespace number
}  // namespace utils
