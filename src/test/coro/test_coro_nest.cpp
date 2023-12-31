/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <coro/coro.h>
#include <wrap/cout.h>
auto& cout = utils::wrap::cout_wrap();

int main() {
    auto c = utils::coro::make_coro();
    c.add_coroutine(nullptr, [](utils::coro::C* c, void*) {
        c->add_coroutine(nullptr, [](utils::coro::C* c, void*) {
            cout << "before suspend 2\n";
            c->suspend();
            cout << "after suspend 2\n";
        });
        cout << "before suspend 1\n";
        c->suspend();
        cout << "after suspend 1\n";
        c->add_coroutine(nullptr, [](utils::coro::C* c, void*) {
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
