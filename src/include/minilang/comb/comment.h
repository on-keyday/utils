/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "comb.h"

namespace utils::minilang::comb::known {
    constexpr auto make_comment(const char* begin, const char* end, bool nest = false) {
        return [=](auto& seq, auto&&, auto&&) {
            if (!seq.seek_if(begin)) {
                return CombStatus::not_match;
            }
            size_t i = 1;
            while (true) {
                if (!end) {
                    if (seq.eos() || seq.seek_if("\r\n") || seq.seek_if("\r") || seq.seek_if("\n")) {
                        break;
                    }
                }
                else {
                    if (seq.eos()) {
                        return CombStatus::fatal;
                    }
                    if (seq.seek_if(end)) {
                        i--;
                        if (i == 0) {
                            break;
                        }
                        continue;
                    }
                }
                if (end && nest) {
                    if (seq.seek_if(begin)) {
                        i++;
                        continue;
                    }
                }
                seq.consume();
            }
            return CombStatus::match;
        };
    }

    constexpr auto c_comment = make_comment("/*", "*/");
    constexpr auto nest_c_comment = make_comment("/*", "*/", true);
    constexpr auto cpp_comment = make_comment("//", nullptr);

    namespace test {
        constexpr bool check_comment() {
            auto do_test = [](const char* text, auto fn, CombStatus req = CombStatus::match) {
                auto seq = make_ref_seq(text);
                if (fn(seq, 0, 0) != req) {
                    return false;
                }
                return true;
            };
            return do_test("/*comment*/", c_comment) &&
                   do_test("/**/", c_comment) &&
                   do_test("// ok?\n", cpp_comment) &&
                   do_test("// ok", cpp_comment) &&
                   do_test("/***/", c_comment) &&
                   do_test("/*/**/", c_comment) &&
                   do_test("/*/**/", nest_c_comment, CombStatus::fatal) &&
                   do_test("/*/*nested*/*/", nest_c_comment) &&
                   do_test("is_ok?", c_comment, CombStatus::not_match);
        }

        static_assert(check_comment());
    }  // namespace test
}  // namespace utils::minilang::comb::known
