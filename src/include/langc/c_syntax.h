/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <comb2/composite/string.h>
#include <comb2/composite/indent.h>
#include <comb2/composite/comment.h>
#include <comb2/basic/group.h>

namespace utils {
    namespace langc {
        using namespace comb2::ops;
        namespace cps = comb2::composite;
        auto lexer() {
            auto comment = cps::c_comment | cps::cpp_comment;
            auto space = cps::space | cps::tab;
            auto c_ignored = ~space | cps::eol | comment;
            auto lex = [&](auto&&... args) {
                return (... | str(args, lit(args)));
            };
            lex("++", "+=", "+", "--", "-=", "-");
        }
    }  // namespace langc
}  // namespace utils
