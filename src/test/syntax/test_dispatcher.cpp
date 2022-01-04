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
    auto t = make_tokenizer();
    auto seq = utils::make_ref_seq(R"()");
    auto m = c->make_tokenizer(seq, t);
    decltype(t)::token_t tok;
    assert(m);
    auto input = utils::make_ref_seq("R()");
    t.symbol.predef.push_back("#");
    t.tokenize(input, tok);
    const char* err = nullptr;
    auto e = tknz::merge(err, tok, tknz::sh_comment(), tknz::string());
    assert(e);
}
