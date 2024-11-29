/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/socket.h>
#include <fnet/addrinfo.h>

int main() {
    auto client = futils::fnet::make_socket(futils::fnet::sockattr_tcp(futils::fnet::ip::Version::ipv6)).value();
    auto server = futils::fnet::make_socket(futils::fnet::sockattr_tcp(futils::fnet::ip::Version::ipv6)).value();

    client.set_ipv6only(false);
    server.set_ipv6only(false);

    constexpr auto address = *futils::fnet::to_ipv6("::ffff:127.0.0.1", 8080, true);

    server.set_reuse_addr(true);
    server.bind(address).value();  // or except

    server.listen().value();  // or except

    bool accepted = false, connected = false, canceled = false;
    futils::fnet::Canceler c;

    server.accept_async(futils::fnet::async_accept_then([&](futils::fnet::Socket&& listener, futils::fnet::Socket&& ok, futils::fnet::NetAddrPort&& port, futils::fnet::NotifyResult res) {
              if (!ok) {
                  throw "accept error";
              }
              accepted = true;
              auto a = listener.accept_async(futils::fnet::async_accept_then([&](futils::fnet::Socket&& listener, futils::fnet::Socket&& a, futils::fnet::NetAddrPort&& port, futils::fnet::NotifyResult res) {
                                   if (a) {
                                       throw "unexpected accept";
                                   }
                                   canceled = true;
                               }))
                           .value();  // or except
              c = std::move(a.cancel);
          }))
        .value();  // or except

    client.connect_async(futils::fnet::async_connect_then(address, [&](futils::fnet::Socket&& socket, futils::fnet::NotifyResult res) {
              if (!res.value()) {
                  throw "connect error";
              }
              connected = true;
          }))
        .value();  // or except

    while (!accepted || !connected) {
        futils::fnet::wait_io_event(1000);
    }

    c.cancel().value();  // or except
    while (!canceled) {
        futils::fnet::wait_io_event(1000);
    }
}
