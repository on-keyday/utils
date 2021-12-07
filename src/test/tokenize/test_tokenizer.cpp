/*license*/

#include "../../include/tokenize/tokenizer.h"

#include "../../include/wrap/lite/string.h"
#include "../../include/wrap/cout.h"
using namespace utils;

void test_tokenizer() {
    tokenize::Tokenizer<wrap::string> tokenizer;
    decltype(tokenizer)::token_t output;

    tokenizer.keyword.predef = {"def", "func", "int", "bool"};
    tokenizer.symbol.predef = {"(", ")", "->"};

    char8_t testword[] =
        u8R"(
        def あれれ int
        func Hey()->bool
    )";

    Sequencer input(testword);

    auto& cout = wrap::cout_wrap();

    auto result = tokenizer.tokenize(input, output);
    assert(result && "expect ture but tokenize is failed");
    for (auto p = output; p; p = p->next) {
        cout << p->what() << ":`" << p->to_string() << "`\n";
    }
}

int main() {
    test_tokenizer();
}