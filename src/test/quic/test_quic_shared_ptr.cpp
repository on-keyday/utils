/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <quic/mem/shared_ptr.h>

struct Object {
    int a;
    bool b;
};

void test_quic_shared_ptr() {
    using namespace utils::quic;
    auto stdalloc = allocate::stdalloc();
    auto ptr = mem::make_shared<Object>(&stdalloc);
    auto v = ptr;
    auto v2 = std::move(ptr);
}

int main() {
    test_quic_shared_ptr();
}
