/*license*/

#include "../../include/syntax/syntaxc/syntaxc.h"
#include "../../include/wrap/lite/vector.h"
#include "../../include/wrap/lite/map.h"

void test_syntaxc() {
    utils::syntax::SyntaxC<utils::wrap::string, utils::wrap::vector, utils::wrap::map> test;

    auto seq =
        u8R"a(
        ROOT:=ID|"("ROOT ["." ROOT|ROOT*]? ")"
    )a";

    utils::Sequencer input(seq);

    utils::tokenize::Tokenizer<utils::wrap::string, utils::wrap::vector> tokenizer;

    auto result = test.make_tokenizer(input, tokenizer);

    assert(result && "expect true but make tokenizer failed");

    auto other =
        u8R"(
        ((a i u).(e o))
    )";

    utils::Sequencer input2(other);

    decltype(tokenizer)::token_t res;

    tokenizer.tokenize(input2, res);
    utils::syntax::Reader<utils::wrap::string> r{res};
    auto result2 = test.matching(r);

    assert(result2 && "expect true but matching is failed");
}

int main() {
    test_syntaxc();
}