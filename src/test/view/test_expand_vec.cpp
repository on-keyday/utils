/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <testutil/alloc_hook.h>
#include <view/expand_iovec.h>

void arg_test(futils::view::rvec d) {}

int main() {
    futils::test::set_alloc_hook(true);
    futils::view::expand_storage_vec<std::allocator<futils::byte>> data;
    data.reserve(17);
    data.push_back('h');
    data.shrink_to_fit();
    auto d = data[0];
    data.reserve(32);
    data.push_back('e');
    data.push_back('l');
    data.push_back('l');
    data.push_back('o');
    auto copy = data;
    auto move = std::move(data);
    copy.append(" world");
    move.shrink_to_fit();
    copy.append(" because of my own policy");
    futils::view::basic_expand_storage_vec<std::allocator<futils::byte>, futils::byte, true> data2;
    data2.reserve(17);
    data2.push_back('h');
    data2.shrink_to_fit();
    auto d2 = data2[0];
    data2.reserve(32);
    data2.push_back('e');
    data2.push_back('l');
    data2.push_back('l');
    data2.push_back('o');
    auto copy2 = data2;
    auto move2 = std::move(data2);
    copy2.append(" world");
    move2.shrink_to_fit();
    copy2.append(" because of my own policy");
    auto s = copy2.c_str();
}
