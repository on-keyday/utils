/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <async/worker.h>
#include <wrap/cout.h>
#include <thread>

void test_worker() {
    using namespace utils;
    async::TaskPool pool;
    std::atomic_size_t count = 0;
    pool.set_priority_mode(true);
    pool.set_maxthread(1);
    auto task = pool.start([&](async::Context& ctx) {
        ctx.task_wait([&](async::Context& ctx) {
            // ctx.cancel();
            ctx.task_wait([&](async::Context& ctx) {
                // ctx.set_priority(10);
                for (auto i = 0; i < 10000; i++) {
                    utils::wrap::cout_wrap() << utils::wrap::pack(std::this_thread::get_id(), ":", i, "\n");
                    ctx.suspend();
                }
                ctx.cancel();
            });
            utils::wrap::cout_wrap() << "hello guy\n";
        });
    });
    pool.post([&](async::Context& ctx) {
        while (!task.is_done()) {
            count++;
            utils::wrap::cout_wrap() << "async\n";
            ctx.suspend();
        }
    });
    auto v = pool.start([](async::Context& ctx) {
        ctx.set_value("That calling convertion is stdcall is not true, but it used to be true.\n");
    });
    task.wait();
    auto result = v.get();
    utils::wrap::cout_wrap() << "done!\n";
    utils::wrap::cout_wrap() << *result.type_assert<const char*>();
}

int main() {
    test_worker();
}
