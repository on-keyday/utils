/*license*/
#include "../../include/wrap/cout.h"

void test_cout() {
    auto& cout = utils::wrap::cout_wrap();

    cout << U"ありがとう!\n";
    cout << 3;
    cout << utils::wrap::pack("\nerror: ", 3, U" is not ", u"a vector\n").pack(u8"please pay money, つまるところ かねはらえ");
}

int main() {
    test_cout();
}
