/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <iface/base_iface.h>
#include <string>

void test_base_iface() {
    using namespace utils::iface;
    String<Owns> str1, mv;
    std::string ref;
    str1 = ref;
    ref = "game over";
    auto ptr = str1.c_str();
    str1.size();
    mv = std::move(str1);
}

int main() {
    test_base_iface();
}
