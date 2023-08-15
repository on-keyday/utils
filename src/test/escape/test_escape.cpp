/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/escape/escape.h"
#include "../../include/helper/pushbacker.h"
#include "../../include/strutil/equal.h"

constexpr auto test_escape_str(const char8_t* str, utils::escape::EscapeFlag flag) {
    namespace ue = utils::escape;
    namespace uh = utils::helper;
    auto seq = utils::make_ref_seq(str);
    uh::FixedPushBacker<char[30], 29> out;
    ue::escape_str(seq, out, flag);
    return out;
}

constexpr auto test_unescape_str(const char* str) {
    namespace ue = utils::escape;
    namespace uh = utils::helper;
    auto seq = utils::make_ref_seq(str);
    uh::FixedPushBacker<char8_t[30], 29> out;
    ue::unescape_str(seq, out);
    return out;
}

void test_escape() {
    constexpr auto e = test_escape_str(u8"\n\t\rあい", utils::escape::EscapeFlag::utf16);
    static_assert(utils::strutil::equal("\\n\\t\\r\\u3042\\u3044", e.buf), "expect true but assertion failed");
    constexpr auto o = test_escape_str(u8"\n\t\rあ", utils::escape::EscapeFlag::hex);
    static_assert(utils::strutil::equal("\\n\\t\\r\\xe3\\x81\\x82", o.buf), "expect true but assertion failed");
    constexpr auto ue = test_unescape_str("\\n\\t\\r\\u3042");
    static_assert(utils::strutil::equal(u8"\n\t\rあ", ue.buf), "expect true but assertion failed");
    constexpr auto t1 = test_escape_str(u8"🎅", utils::escape::EscapeFlag::utf16);
    constexpr auto t2 = test_unescape_str(t1.buf);
    static_assert(utils::strutil::equal(u8"🎅", t2.buf), "expect true but assertion failed");
}

int main() {
    test_escape();
}
