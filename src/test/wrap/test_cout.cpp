/*license*/
#include "../../include/wrap/cout.h"

void test_cout() {
    auto& cout = utils::wrap::cout_wrap();

    cout << U"ありがとう!\n";
    cout << 3;
    auto pack = utils::wrap::pack("error:", 3, U" is not ", u"a vector");
    pack.pack(u8"please pay money\n");
    cout << std::move(pack);
}

int main() {
    test_cout();
}
