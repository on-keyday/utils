/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "../../include/wrap/cout.h"

#include <functional>

void test_cout() {
    auto& cout = futils::wrap::cout_wrap();

    cout << U"ありがとう!\n";
    cout << 3;
    cout << futils::wrap::pack("\nerror: ", 3, U" is not ", u"a vector\n")
                .pack(u8"please pay money, つまるところ かねはらえ\n", std::hex, 12);
    std::function<int(int)> f;
}

int main() {
    test_cout();
}
