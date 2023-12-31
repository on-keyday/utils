/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/core/sequencer.h"

#include <string>

constexpr char test_core_constexpr() {
    utils::Sequencer charbuf("Hello World");
    static_assert(std::is_same_v<decltype(charbuf), utils::Sequencer<const char*>>, "Sequencer type is wrong");
    using SeqType = decltype(charbuf);
    static_assert(std::is_same_v<SeqType::char_type, char>, "char_type is not char");
    charbuf.seek_if("Hello");
    auto result = charbuf.current();
    return result;
}

void test_core() {
    constexpr char constresult = test_core_constexpr();
    static_assert(constresult == ' ', "constexpr is failed");
    utils::Sequencer strbuf_normal(std::string("data"));
    static_assert(std::is_same_v<decltype(strbuf_normal), utils::Sequencer<std::string>>, "Sequencer type is wrong");
    std::string buffer;
    utils::Sequencer strbuf_ref(buffer);
    static_assert(std::is_same_v<decltype(strbuf_ref), utils::Sequencer<std::string&>>, "Sequencer type is wrong");
    utils::Sequencer<std::string> strbuf("string buffer");
    bool result = strbuf.match("string buffer");
    assert(result && "result must be true");
    auto ref = utils::make_ref_seq(buffer);
}

int main() {
    test_core();
}
