/*license*/

#include "../../include/tokenize/tokenizer.h"

#include "../../include/wrap/lite/string.h"
using namespace utils;

void test_tokenizer() {
    tokenize::Tokenizer<wrap::string> tokenizer;
    decltype(tokenizer)::token_t token;

    tokenizer.keyword.predef = {"def", "func", "int", "bool"};
    tokenizer.symbol.predef = {"(", ")", "->"};

    char8_t testword[] =
        u8R"(
        def あれれ int
        func Hey()->bool
    )";

    Sequencer input(testword);

    tokenizer.tokenize(input, token);
}