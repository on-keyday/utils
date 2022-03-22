/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <async/light/make_arg.h>
using namespace utils::async::light;

struct V {
    int val;
    int v;
};

const auto& as_const(auto& v) {
    return v;
}

void test_make_arg(V& arg) {
    int object = 0;
    auto m = make_arg(9, "hello", std::move(object), arg, "boke");
    auto a = make_arg(as_const(object), object, 0.3);
    a.invoke([](int m, int n, double f) {
    });
}

int main() {}