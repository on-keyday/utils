/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <async/light/context.h>
#include <wrap/cout.h>
#include <thread>
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
    auto make_routine = []() {
        return start<int>(true, [](Context<int> ctx) {
            int obj = 4;
            ctx.yield(obj);
            auto v = ctx.await(start<const char*>(true, [](Context<const char*> ctx) {
                cout << "inner await\n";
                auto t = ctx.await(start<bool>(true, []() {
                    cout << "deep inner await\n";
                    return true;
                }));
                return "hello world";
            }));
            cout << v << "\n";
            return 0;
        });
    };
    auto j = std::thread([f = make_routine()]() mutable {
        auto v = f.get();
        assert(v == 4);
        f.resume();
        v = f.get();
        assert(v == 0);
        auto b = f.resume();
        assert(b == false);
        v = f.get();
        assert(v == 0);
    });
    auto v = make_routine();
    auto s = v.get();
    v.resume();
    s = v.get();
    j.join();
    v.rebind_function([](int a) { return a; }, 53);
    s = v.get();
    assert(s == 53);
    v.rebind_function([](Context<int> ctx) { return 0; });
}

int main() {
    test_shared_context();
}
