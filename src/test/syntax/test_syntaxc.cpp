/*license*/

#include "../../include/syntax/syntaxc/syntaxc.h"

#include "../../include/syntax/syntaxc/make_syntaxc.h"
#include "../../include/wrap/cout.h"

void test_syntaxc() {
    auto& cout = utils::wrap::cout_wrap();
    utils::syntax::SyntaxC<utils::wrap::string, utils::wrap::vector, utils::wrap::map> test;
    test.cb = [&](auto& r) {
        cout << utils::syntax::keywordv(r.kind) << "-";
        cout << r.token;
        cout << "\n";
    };
    auto seq =
        u8R"a(
        ROOT:=IDVAR|EXPR
        JSON:="{" OBJPARAM "}"|"["JSON ["," JSON]*? "]"|NUMBER|STRING|"null"|"true"|"false"
        OBJPARAM:=STRING ":" JSON ["," OBJPARAM]?

        IDVAR:=ID ":=" NUMBER
        EXPR:= ID "+" NUMBER
    )a";

    utils::Sequencer input(seq);

    utils::tokenize::Tokenizer<utils::wrap::string, utils::wrap::vector> tokenizer;

    auto result = test.make_tokenizer(input, tokenizer);

    assert(result && "expect true but make tokenizer failed");

    auto other =
        u8R"(


       {
           "hello":"world",
           "hey": [null,"string",0.2,3],
           "google":true,
           "gmail":"com"
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
