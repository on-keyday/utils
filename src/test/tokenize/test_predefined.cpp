/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/tokenize/predefined.h"

#include "../../include/wrap/lite/vector.h"
#include "../../include/wrap/lite/string.h"

using namespace utils;

void test_predefined() {
    tokenize::Predefined<wrap::string, wrap::vector> keyword, symbol;
    keyword.predef = {"test", "define"};
    symbol.predef = {"!"};

    char teststr[] = "test!defined";
    Sequencer seq(teststr);
    wrap::string matched;

    bool result = tokenize::read_predefined<true>(seq, keyword, matched, symbol);
    assert(result && "read_predefined is incorrect");
    assert(matched == "test" && "expect 'test' but not");

    result = tokenize::read_predefined<false>(seq, symbol, matched);
    assert(result && "read_predefined is incorrect");
    assert(matched == "!" && "expect 'test' but not");

    result = tokenize::read_predefined<true>(seq, keyword, matched, symbol);
    assert(result == false && "read_predefined is incorrect");
}

int main() {
    test_predefined();
}
