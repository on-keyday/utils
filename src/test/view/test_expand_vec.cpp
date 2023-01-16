/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <testutil/alloc_hook.h>
#include <view/expand_iovec.h>

void arg_test(utils::view::rvec d) {}

int main() {
    utils::test::set_alloc_hook(true);
    utils::view::expand_storage_vec<std::allocator<utils::byte>> data;
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
}
