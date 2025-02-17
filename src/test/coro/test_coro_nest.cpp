/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <coro/coro.h>
#include <wrap/cout.h>
auto& cout = futils::wrap::cout_wrap();

int main() {
    auto c = futils::coro::make_coro();
    c.add_coroutine(nullptr, [](futils::coro::C* c, void*) {
        c->add_coroutine(nullptr, [](futils::coro::C* c, void*) {
            cout << "before suspend 2\n";
            c->suspend();
            cout << "after suspend 2\n";
        });
        cout << "before suspend 1\n";
        c->suspend();
        cout << "after suspend 1\n";
        c->add_coroutine(nullptr, [](futils::coro::C* c, void*) {
            cout << "before suspend 3\n";
            c->suspend();
            cout << "after suspend 3\n";
        });
    });
    cout << "enter main\n";
    while (c.run()) {
        ;
    }
    cout << "leave main\n";
}
