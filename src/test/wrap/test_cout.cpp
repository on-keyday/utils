/*license*/
#include "../../include/wrap/cout.h"

void test_cout() {
    auto& cout = utils::wrap::cout_wrap();

    cout << U"ありがとう!\n";
    cout << 3;
}

int main() {
    test_cout();
}
