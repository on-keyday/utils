/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <async/light/context.h>
#include <wrap/cout.h>
using namespace utils::async::light;
auto& cout = utils::wrap::cout_wrap();

void test_shared_context() {
    auto ptr = make_shared_context<int>();
    int refobj = 0;
    ptr->replace_function(
        [](const int& obj) -> int {
            return obj;
        },
        std::move(refobj));
    ptr->invoke();
    ptr->replace_function([]() -> int {
        cout << "called\n";
        return 0;
    });
    ptr->invoke();
    auto f = invoke<int>([](Context<int> ctx) {
        int obj = 4;
        ctx.yield(obj);
        auto v = ctx.await(start<const char*>(true, [](Context<const char*> ctx) {
            return "hello world";
        }));
        cout << v << "\n";
        return 0;
    });
    auto v = f.get();
    assert(v == 4);
    f.resume();
    v = f.get();
    assert(v == 0);
    auto b = f.resume();
    assert(b == false);
    v = f.get();
    assert(v == 0);
}

int main() {
    test_shared_context();
}
