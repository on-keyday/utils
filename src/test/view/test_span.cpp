/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <view/span.h>
#include <cassert>
#include <vector>

int main() {
    assert(futils::view::test::check_vspan());
    futils::view::vspan<int> obj;
    obj = std::vector<int>();
}