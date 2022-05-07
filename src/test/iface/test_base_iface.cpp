/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <iface/base_iface.h>
#include <string>
#include <cassert>

void test_base_iface() {
    using namespace utils::iface;
    String<Sized<Owns>> str1, mv;
    std::string ref;
    str1 = ref;
    ref = "game over";
    auto ptr = str1.c_str();

    mv = std::move(str1);
    Buffer<PushBacker<String<Ref>>> buf;
    buf = ref;
    sizeof(buf);
    buf.push_back('!');
    assert(ref == "game over!");

    auto val = buf;
    val.pop_back();
}

int main() {
    test_base_iface();
}
