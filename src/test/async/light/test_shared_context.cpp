/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <async/light/context.h>
using namespace utils::async::light;

void test_shared_context() {
    auto ptr = make_shared_context<int>();
    int refobj = 0;
    ptr->replace_function(
        [](int& obj) {
            return obj;
        },
        refobj);
    ptr->invoke();
}

int main() {
    test_shared_context();
}