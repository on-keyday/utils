/*license*/
#include "../../include/wrap/cout.h"

void test_cout() {
    auto& cout = utils::wrap::cout_wrap();

    cout << "strings";
}

int main() {
}