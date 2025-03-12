/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <helper/variant.h>
#include <cassert>
#include <variant>

int global_var = 0;

struct modify_global {
    ~modify_global() {
        global_var = 1;
    }
};

void test_storage() {
    futils::helper::alt::variant_storage<modify_global, int> v{32};
    assert(v.index() == 1);
    assert(v.get<int>() != nullptr);
    assert(*v.get<int>() == 32);
    v = modify_global{};
    assert(v.index() == 0);
    assert(v.get<modify_global>() != nullptr);
}

int main() {
    assert(futils::helper::alt::test::many_of_storage());
    test_storage();
    assert(global_var == 1);
    using namespace futils::helper::alt::test;
    std::variant<MANY_TYPES> v;
}
