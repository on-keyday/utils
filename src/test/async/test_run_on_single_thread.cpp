/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <async/worker.h>
#include <wrap/cout.h>
using namespace utils;

void test_run_signle_thread() {
    async::TaskPool pool;
    pool.set_maxthread(0);
    pool.set_yield(true);

    auto f = async::start(pool, [](async::Context& ctx) {
        for (auto i = 0; i < 100000; i++) {
            wrap::cout_wrap() << "count:" << i << "\n";
            ctx.suspend();
        }
    });

    async::start(
        pool, [](async::Context& ctx, async::AnyFuture f) {
            while (!f.is_done()) {
                wrap::cout_wrap() << "async but single thread\n";
                ctx.suspend();
            }
        },
        std::move(f));

    pool.run_on_this_thread();
}

int main() {
    test_run_signle_thread();
}
