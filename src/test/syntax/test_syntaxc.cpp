/*license*/

#include "../../include/syntax/syntaxc/syntaxc.h"

#include "../../include/syntax/syntaxc/make_syntaxc.h"

void test_syntaxc() {
    utils::syntax::SyntaxC<utils::wrap::string, utils::wrap::vector, utils::wrap::map> test;
    test.cb = "";
    auto seq =
        u8R"a(
        ROOT:=JSON
        JSON:="{" OBJPARAM*? "}"|STRING
        OBJPARAM:=STRING ":" JSON
    )a";

    utils::Sequencer input(seq);

    utils::tokenize::Tokenizer<utils::wrap::string, utils::wrap::vector> tokenizer;

    auto result = test.make_tokenizer(input, tokenizer);

    assert(result && "expect true but make tokenizer failed");

    auto other =
        u8R"(
       {
           "hello":"world"
       }
    )";

    utils::Sequencer input2(other);

    decltype(tokenizer)::token_t res;

    tokenizer.tokenize(input2, res);

    utils::tokenize::merge(res, utils::tokenize::escaped_comment<utils::wrap::string>("\"", "\\"));

    utils::syntax::Reader<utils::wrap::string> r{res};
    auto result2 = test.matching(r);

    assert(result2 && "expect true but matching is failed");
}

int main() {
    test_syntaxc();
}
