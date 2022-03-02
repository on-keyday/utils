/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <async/coroutine/task.h>

using namespace utils;
async::coro::Task<bool> task2() {
    co_return true;
}

async::coro::Task<int> task() {
    auto v = task2();
    auto v = v.get();
}

void test_coroutine() {
}