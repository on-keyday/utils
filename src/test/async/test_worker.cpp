/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <async/worker.h>
#include <wrap/cout.h>

void test_worker() {
    using namespace utils;
    async::TaskPool pool;
    std::atomic_flag done;
    done.clear();
    pool.post([&](async::Context& ctx) {
        ctx.wait_task([](async::Context& ctx) {
            //ctx.cancel();
            ctx.wait_task([](async::Context& ctx) {
                for (auto i = 0; i < 10000; i++) {
                    utils::wrap::cout_wrap() << utils::wrap::pack(std::this_thread::get_id(), ":", i, "\n");
                    ctx.suspend();
                }
            });
            utils::wrap::cout_wrap() << "hello guy\n";
        });
        done.test_and_set();
        done.notify_all();
    });
    pool.post([&](async::Context& ctx) {
        while (done.test() == false) {
            utils::wrap::cout_wrap() << "async\n";
            ctx.suspend();
        }
    });
    done.wait(false);
    utils::wrap::cout_wrap() << "done!\n";
}

int main() {
    test_worker();
}
