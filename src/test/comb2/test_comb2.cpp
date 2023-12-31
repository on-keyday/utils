/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <comb2/composite/number.h>
#include <comb2/basic/unicode.h>
#include <comb2/composite/string.h>
#include <comb2/basic/group.h>

int main() {
    utils::comb2::ops::test::check_unicode();
    constexpr auto c = utils::comb2::ops::str("a", utils::comb2::ops::lit('A'));
}