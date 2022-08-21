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
    utils::test::Timer timer;
    auto pool = make_shared<TaskPool>();
    std::atomic_size_t hit, not_hit, reached;
    auto hc = std::thread::hardware_concurrency();
    for (auto i = 0; i < hc * 10000; i++) {
        cout << "called " << i << "\n"
             << "done\n";
    }
    auto single = timer.next_step<std::chrono::seconds>();
    for (auto i = 0; i < hc; i++) {
        std::thread(
            [&](shared_ptr<TaskPool> pool) {
                SearchContext<Task*> sctx;
                bool first = false;
                TimeContext ctx;
                ctx.wait = std::chrono::milliseconds(100);
                ctx.update_delta = std::chrono::milliseconds(500);
                while (!first || pool->size()) {
                    if (!pool->run_task_with_wait(
                            ctx, [](TimeContext<>& ctx) {
                                size_t hit = ctx.hit_delta();
                                size_t nhit = ctx.not_hit_delta();
                                size_t hit_per_not = nhit - hit;
                                if (nhit > hit && hit_per_not) {
                                    if (hit_per_not > 40) {
                                        ctx.wait *= 2;
                                    }
                                    if (hit_per_not > 10) {
                                        ctx.wait *= 2;
                                    }
                                    ctx.wait *= 4;
                                }
                            },
                            &sctx)) {
                        not_hit++;
                    }
                    else {
                        first = true;
                        hit++;
                    }
                }
                cout << pack(
                    "final ", std::this_thread::get_id(), "\n",
                    "wait time:", ctx.wait.count(), "\n",
                    "hit count:", ctx.hit, "\n",
                    "not hit count:", ctx.not_hit, "\n\n");
                reached++;
            },
            pool)
            .detach();
    }

    for (auto i = 0; i < hc; i++) {
        pool->append(start<void>(
            true, [&pool](Context<void> ctx, int idx) {
                for (auto i = 0; i < 10000; i++) {
                    cout << packln(std::this_thread::get_id(), ": called ", idx, ":", i);
                    ctx.suspend();
                    ctx.await<void>([]() {
                        cout << "do!\n";
                    });
                }
            },
            std::move(i)));
    }
    while (hc > reached) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    cout << "time:" << timer.delta<std::chrono::seconds>().count() << "\n";
    pool->clear();

    cout << "single time:" << single.count() << "\n";
    cout << "hit count:" << hit << "\n";
    cout << "not hit count:" << not_hit << "\n";
}

int main() {
    test_taskpool2();
}
