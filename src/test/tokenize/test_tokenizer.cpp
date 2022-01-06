/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/tokenize/tokenizer.h"
#include "../../include/tokenize/cast.h"
#include "../../include/tokenize/merge.h"

#include "../../include/wrap/lite/string.h"
#include "../../include/wrap/lite/vector.h"
#include "../../include/wrap/cout.h"
using namespace utils;

void test_tokenizer() {
    tokenize::Tokenizer<wrap::string, wrap::vector> tokenizer;
    decltype(tokenizer)::token_t output;

    tokenizer.keyword.predef = {"def", "func", "int", "bool"};
    tokenizer.symbol.predef = {"(", ")", "->", "/*", "*/", "\"", "\\"};

    char8_t testword[] =
        u8R"(
        /* this is comment */
        "/* this is string */ \" escaped "
        def あれれ int
        def define int
        func Hey()->bool
    )";
    auto& cout = wrap::cout_wrap();

    cout << "base string:\n"
         << testword << "\n";

    Sequencer input(testword);

    auto result = tokenizer.tokenize(input, output);
    assert(result && "expect ture but tokenize is failed");

    cout << "result:\n";
    for (auto p = output; p; p = p->next) {
        cout << p->what() << ":`" << p->to_string() << "`\n";
    }

    result = tokenize::merge(output,
                             tokenize::blcok_comment<std::string>("/*", "*/"),
                             tokenize::escaped_comment<std::string>("\"", "\\"));
    assert(result && "expect true but merge is failed");

    cout << "\nmerged:\n";
    for (auto p = output; p; p = p->next) {
        cout << p->what() << ":`" << p->to_string() << "`\n";
    }
}

int main() {
    test_tokenizer();
}
