/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <async/light/pool.h>
#include <thread>
#include <wrap/cout.h>
#include <testutil/timer.h>
using namespace utils::async::light;
using namespace utils::wrap;
auto& cout = cout_wrap();

void test_taskpool2() {
    auto pool = make_shared<TaskPool>();
    std::atomic_size_t hit, not_hit;
    for (auto i = 0; i < std::thread::hardware_concurrency(); i++) {
        std::thread(
            [&](shared_ptr<TaskPool> pool) {
                SearchContext<Task*> sctx;
                bool first = false;
                utils::test::Timer t;
                auto w = std::chrono::milliseconds(12800);
                while (!first || pool->size()) {
                    if (!pool->run_task(&sctx)) {
                        not_hit++;
                        std::this_thread::sleep_for(w);
                    }
                    else {
                        first = true;
                        hit++;
                    }
                }
            },
            pool)
            .detach();
    }
    for (auto i = 0; i < 10; i++) {
        pool->append(start<void>(
            true, [&pool](Context<void> ctx, int idx) {
                for (auto i = 0; i < 10000; i++) {
                    cout << packln(std::this_thread::get_id(), ": called ", idx, ":", i);
                    ctx.suspend();
                    ctx.await(start<void>(true, []() {
                        cout << "do!\n";
                    }));
                }
            },
            std::move(i)));
    }
    while (pool->size()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    pool->clear();

    cout << "hit count:" << hit << "\n";
    cout << "not hit count:" << not_hit << "\n";
}

int main() {
    test_taskpool2();
}
