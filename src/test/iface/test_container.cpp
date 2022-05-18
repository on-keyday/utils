/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <iface/container.h>
#include <iface/storage.h>
#include <deque>
#include <string>
#include <wrap/cout.h>
auto& cout = utils::wrap::cout_wrap();

void test_container() {
    using namespace utils;
    using stringlike = iface::Subscript<iface::Sized<iface::Ref>>;

    using pair_t = iface::Pair<stringlike, stringlike, iface::Powns>;

    using Test = iface::HpackQueue<pair_t, iface::Powns>;

    using base_container = std::deque<pair_t>;
    Test queue = base_container{};

    queue.push_front(std::pair{std::string{"hello"}, std::string{"world"}});

    for (auto& i : queue) {
        auto key = get<0>(i);
        auto value = get<1>(i);
        cout << key << ":" << value << "\n";
    }
}

int main() {
    test_container();
}