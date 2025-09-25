/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <comb2/composite/number.h>
#include <comb2/basic/unicode.h>
#include <comb2/composite/string.h>
#include <comb2/basic/group.h>
#include <comb2/composite/cmdline.h>
#include <vector>
#include <string_view>

int main() {
    futils::comb2::ops::test::check_unicode();
    constexpr auto c = futils::comb2::ops::str("a", futils::comb2::ops::lit('A'));
    futils::comb2::cmdline::command_line<std::vector<std::string_view>>(std::string_view("hello \"world\""), [](auto&) {});
}