/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <async/coroutine/task.h>
#include <async/worker.h>
#include <wrap/cout.h>
using namespace utils;
#ifdef UTILS_COROUTINE_NAMESPACE

async::TaskPool pool;

async::coro::Task<int> task() {
    pool.set_yield(true);
    auto t = pool.start<int>([](async::Context& ctx) {
        auto i = 0;
        for (; i < 100000; i++) {
            wrap::cout_wrap() << i << "\n";
            ctx.suspend();
        }
        ctx.set_value(i);
    });
    int v = co_await t;
    wrap::cout_wrap() << "done\n";
    co_return v;
}

#else
#warning "coroutine not compiled"
async::Future<int> task() {
    return nullptr;
}
#endif

auto test_coroutine() {
    auto t = task();
    wrap::cout_wrap() << "ask task\n";
    return t;
}

int main() {
    auto coro = test_coroutine();
    coro.get();
}
