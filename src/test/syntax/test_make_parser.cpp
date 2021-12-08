/*license*/

#include "../../include/syntax/make_parser/parse.h"
#include "../../include/syntax/make_parser/tokenizer.h"

#include "../../include/wrap/lite/string.h"

using namespace utils;

void test_make_parser() {
    auto tokenizer = utils::syntax::make_tokenizer<wrap::string>();
    decltype(tokenizer)::token_t output;
    char8_t teststr[] =
        u8R"(
        ROOT:="Hey"  [WHAT*]?
        WHAT:="What" [NAME|"hey"]  "?"
        NAME:="your""name"
    )";
    Sequencer input(teststr);
    auto result = tokenizer.tokenize(input, output);
    assert(result && "expect true but tokenize is failed");
}
