/*license*/

#include "../../include/thread/channel.h"
#include <cassert>
#include <thread>
#include <iostream>

void write_thread(utils::thread::SendChan<int> w) {
    for (auto i = 0; i < 10000; i++) {
        w << std::move(i);
        _sleep(5);
    }
    w.close();
}

void test_channecl() {
    {
        auto [w, r] = utils::thread::make_chan<int>(5);
        w << 32;
        int result;
        r >> result;
        assert(result == 32 && "channel is incorrect");
        w << 1;
        w << 2;
        w << 3;
        w << 4;
        w << 5;
        auto result2 = w << 6;
        assert(result2 == utils::thread::ChanStateValue::full && "expect full but not");
    }
    {
        auto [w, r] = utils::thread::make_chan<int>(6);
        r.set_blocking(true);
        w.set_blocking(true);
        std::thread(write_thread, w).detach();
        int v;
        while (r >> v) {
            std::cout << v << "\n";
        }
    }
}

int main() {
    test_channecl();
}