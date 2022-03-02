/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <async/coroutine/task.h>
#ifdef UTILS_COROUTINE_NAMESPACE

using namespace utils;
async::coro::Task<bool> task2() {
    co_return true;
}

async::coro::Task<int> task() {
    auto v = task2();
    auto u = v.get();
    co_return 0;
}
#else
void task() {}
#endif
void test_coroutine() {
    task();
}

int main() {
    test_coroutine();
}