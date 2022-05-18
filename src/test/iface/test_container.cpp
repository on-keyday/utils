/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <iface/container.h>
#include <iface/storage.h>

void test_container() {
    using namespace utils;
    using stringlike = iface::Subscript<iface::Sized<iface::Ref>>;

    using Test = iface::HpackQueue<iface::Pair<stringlike, stringlike, iface::Powns>, iface::Powns>;
    Test queue;
    auto& v = *queue.begin();

    for (auto&& i : queue) {
        auto v = get<0>(i);
    }
}

int main() {
    test_container();
}