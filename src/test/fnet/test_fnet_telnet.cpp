/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/connect.h>
#include <fstream>
namespace fnet = futils::fnet;

int main() {
    auto [sock, addr] = fnet::connect("koukoku.shadan.open.ad.jp", "23", fnet::sockattr_tcp()).value();
    futils::byte data[2000]{};
    std::string buffer;
    size_t count = 0;
    for (;;) {
        auto recv = sock.read(data);
        if (!recv) {
            if (fnet::isSysBlock(recv.error())) {
                if (count >= 100000000) {
                    break;
                }
                count++;
                continue;
            }
            if (recv.error() == fnet::error::eof) {
                break;
            }
            recv.value();
        }
        count = 0;
        buffer.append(recv->as_char(), recv->size());
    }
    std::ofstream ofs("./ignore/from_telnet.txt");
    ofs << buffer;
    return 0;
}
