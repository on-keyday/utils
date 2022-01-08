/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/escape/escape.h"
#include "../../include/helper/pushbacker.h"
#include "../../include/helper/equal.h"

constexpr auto test_escape_str() {
    namespace ue = utils::escape;
    namespace uh = utils::helper;
    auto seq = utils::make_ref_seq(u8"\n\t\rあ");
    uh::FixedPushBacker<char[30], 29> out;
    ue::escape_str(seq, out, ue::EscapeFlag::utf);
    return out;
}

void test_escape() {
    constexpr auto e = test_escape_str();
    static_assert(utils::helper::equal("\\n\\t\\r\\u3042", e.buf), "expect true but assertion failed");
}

int main() {
    test_escape();
}