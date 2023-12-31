/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <view/span.h>
#include <cassert>
#include <vector>

int main() {
    assert(utils::view::test::check_vspan());
    utils::view::vspan<int> obj;
    obj = std::vector<int>();
}