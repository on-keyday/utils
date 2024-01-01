/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/thread/channel.h"
#include <cassert>
#include <thread>
#include "../../include/wrap/cout.h"
#include "../../include/thread/fork_channel.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#define Sleep sleep
#endif

auto& cout = futils::wrap::cout_wrap();

void write_thread(futils::thread::SendChan<int> w) {
    for (auto i = 0; i < 10000; i++) {
        w << std::move(i);
        //_sleep(5);
    }
    w.close();
}

void recv_thread(size_t index, futils::thread::RecvChan<int> r, futils::wrap::shared_ptr<futils::thread::Subscriber<int>> ptr) {
    r.set_blocking(true);
    int v;
    while (r >> v) {
        cout << futils::wrap::pack(std::this_thread::get_id(), ":", index, ":", v, "\n");
    }
}

void dummmy_message(futils::thread::ForkChan<int> fork) {
    //fork.set_blocking(futils::thread::BlockLevel::mustnot);
    while (!fork.is_closed()) {
        cout << futils::wrap::pack(std::this_thread::get_id(), ":dummy\n");
    }
}

void test_channecl() {
    {
        auto [w, r] = futils::thread::make_chan<int>(5);
        w << 32;
        int result = 0;
        r >> result;
        assert(result == 32 && "channel is incorrect");
        w << 1;
        w << 2;
        w << 3;
        w << 4;
        w << 5;
        auto result2 = w << 6;
        assert(result2 == futils::thread::ChanStateValue::full && "expect full but not");
    }
    {
        auto [w, r] = futils::thread::make_chan<int>(6);
        r.set_blocking(true);
        w.set_blocking(true);
        std::thread(write_thread, w).detach();
        int v;
        while (r >> v) {
            cout << futils::wrap::pack(v, "\n");
        }
    }
    {
        auto fork = futils::thread::make_forkchan<int>();
        for (auto i = 0; i < 5; i++) {
            auto [w, r] = futils::thread::make_chan<int>(5);
            auto sub = fork.subscribe(std::move(w));
            std::thread(recv_thread, i, r, sub).detach();
        }
        fork.set_blocking(true);
        std::thread(dummmy_message, fork).detach();
        for (auto i = 0; i < 10000; i++) {
            fork << std::move(i);
        }
        fork.close();

        Sleep(200);
    }
}

int main() {
    test_channecl();
}
