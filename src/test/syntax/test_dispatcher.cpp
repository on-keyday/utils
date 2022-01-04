/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "../../include/syntax/dispatcher/default_dispatcher.h"
#include "../../include/syntax/syntaxc/make_syntaxc.h"
#include "../../include/tokenize/merger.h"

void test_dispatcher() {
    using namespace utils::syntax;
    DefaultDispatcher disp;

    auto c = make_syntaxc();
    auto seq = utils::make_ref_seq(R"(
        COMMENT_TAG:="#"
    )");
    auto input = utils::make_ref_seq(R"(

    )");
    auto tok = default_parse(c, seq, input, tknz::sh_comment(), tknz::string());
}
