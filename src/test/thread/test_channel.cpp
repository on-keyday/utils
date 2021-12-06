/*license*/

#include "../../include/thread/channel.h"
#include <cassert>

void test_channecl() {
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

int main() {
    test_channecl();
}