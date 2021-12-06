/*license*/
#include "../../include/wrap/cout.h"

void test_cout() {
    auto& cout = utils::wrap::cout_wrap();

    cout << U"ありがとう!\n";
    cout << 3;
    auto pack = utils::wrap::pack("\nerror: ", 3, U" is not ", u"a vector\n");
    pack.pack(u8"please pay かねはらえ");
    cout << std::move(pack);
}

int main() {
    test_cout();
}
