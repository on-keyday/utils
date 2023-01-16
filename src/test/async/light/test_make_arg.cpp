/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <async/light/make_arg.h>
#include <async/light/context.h>
#include <memory>
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
    auto a = make_arg(as_const(object), std::ref(object), 0.3);
    a.invoke([](int&& m, int& n, const double& f) {
        auto arg = make_arg(std::move(m), n, f);
    });
    auto record = make_funcrecord([](int) {
        return false;
    },
                                  object);

    auto ptr = std::make_shared<decltype(record)>(std::move(record));
}

int main() {
    V v;
    test_make_arg(v);
}
