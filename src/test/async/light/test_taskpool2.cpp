/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <async/light/pool.h>
#include <thread>
#include <wrap/cout.h>
using namespace utils::async::light;
using namespace utils::wrap;
auto& cout = cout_wrap();

void test_taskpool2() {
    auto pool = make_shared<TaskPool>();
    for (auto i = 0; i < std::thread::hardware_concurrency() / 2; i++) {
        std::thread(
            [](shared_ptr<TaskPool> pool) {
                while (true) {
                    if (!pool->run_task()) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                }
            },
            pool)
            .detach();
    }
    for (auto i = 0; i < 10; i++) {
        pool->append(start<void>(
            true, [&pool](Context<void> ctx, int idx) {
                for (auto i = 0; i < 1000; i++) {
                    cout << packln(std::this_thread::get_id(), ": called ", idx, ":", i);
                    ctx.suspend();
                    pool->append(start<void>(true, []() {
                        cout << "do!\n";
                    }));
                }
            },
            std::move(i)));
    }
    while (pool->size()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int main() {
    test_taskpool2();
}
