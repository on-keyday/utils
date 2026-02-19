/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/socket.h>
#include <fnet/connect.h>
#include <array>
#include <thread>
#include <wrap/cout.h>
auto& cout = futils::wrap::cout_wrap();

void accept_thread() {
    auto w = futils::fnet::get_self_server_address("8080", futils::fnet::sockattr_udp(futils::fnet::ip::Version::ipv4), "localhost").and_then([](auto&& w) {
        return w.wait();
    });
    if (!w) {
        cout << futils::wrap::pack("failed to get server address: ", w.error().error(), "\n");
        return;
    }
    futils::fnet::Socket sock;
    while (w->next()) {
        auto a = w->sockaddr();
        auto s = futils::fnet::make_socket(a.attr).and_then([&](auto&& s) -> futils::fnet::expected<futils::fnet::Socket> {
            return s.bind(a.addr)
                .transform([&] {
                    return std::move(s);
                });
        });
        if (!s) {
            cout << futils::wrap::pack("failed to create socket: ", s.error().error(), "\n");
            continue;
        }
        sock = std::move(*s);
    }
    if (!sock) {
        cout << "failed to create socket\n";
        return;
    }
    cout << "listening\n";
    while (true) {
        futils::byte buf[3000];
        auto res = sock.readfrom(buf);
        if (!res) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        auto [r, addr] = std::move(*res);
        cout << futils::wrap::pack("received: ", r, "\n");
        sock.writeto(addr, r);
    }
}

void single_thread(futils::fnet::Socket& sock, futils::fnet::SockAddr& addr) {
    // on same thread, works well
    for (size_t i = 0; i < 10; i++) {
        cout << futils::wrap::pack("i=", i, "\n");
        futils::fnet::DeferredCallback outer_cb;
        auto r = sock.readfrom_async_deferred(futils::fnet::async_notify_addr_then(
            futils::fnet::BufferManager<std::array<std::uint8_t, 3000>>{}, [&](futils::fnet::DeferredCallback&& cb) { outer_cb = std::move(cb); },
            [](futils::fnet::Socket&& sock, auto&& buf, futils::fnet::NetAddrPort&& addr, futils::fnet::NotifyResult&& r) {
                if (r.value()) {
                    cout << "received\n";
                }
                else {
                    cout << "cancelled?: " << r.value().error().error() << "\n";
                }
            }));
        if (!r) {
            return;
        }

        r->cancel.cancel();

        while (true) {
            futils::fnet::wait_io_event(1);
            if (outer_cb) {
                outer_cb.invoke();
                break;
            }
        }
    }
}

void multi_thread(futils::fnet::Socket& sock, futils::fnet::SockAddr& addr) {
    std::thread(accept_thread).detach();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    futils::fnet::Canceler cancel;
    for (size_t i = 0; i < 1000; i++) {
        cout << futils::wrap::pack("i=", i, "\n");
        futils::fnet::DeferredCallback outer_cb;
        constexpr auto addr = futils::fnet::ipv4(127, 0, 0, 1, 8080);
        auto r = sock.readfrom_async_deferred(futils::fnet::async_notify_addr_then(
            futils::fnet::BufferManager<std::array<std::uint8_t, 3000>>{}, [&](futils::fnet::DeferredCallback&& cb) { outer_cb = std::move(cb); },
            [](futils::fnet::Socket&& sock, auto&& buf, futils::fnet::NetAddrPort&& addr, futils::fnet::NotifyResult&& r) {
                if (r.value()) {
                    cout << "received\n";
                }
                else {
                    cout << futils::wrap::pack("cancelled?: ", r.value().error().error(), "\n");
                }
            }));
        if (!r) {
            cout << futils::wrap::pack("failed to readfrom_async: ", r.error().error(), "\n");
            continue;
        }

        if (!cancel) {
            cancel = std::move(r->cancel);
        }
        else {
            std::thread(
                [](futils::fnet::Canceler&& canceler1, futils::fnet::Canceler&& canceler2) {
                    auto res = canceler1.cancel();
                    if (!res) {
                        cout << futils::wrap::pack("failed to cancel: ", res.error().error(), "\n");
                    }
                    else {
                        cout << "cancelled\n";
                    }
                    res = canceler2.cancel();
                    if (!res) {
                        cout << futils::wrap::pack("failed to cancel: ", res.error().error(), "\n");
                    }
                    else {
                        cout << "cancelled\n";
                    }
                },
                std::move(r->cancel), std::move(cancel))
                .detach();
        }

        auto res = sock.writeto(addr, "hello");
        if (!res) {
            cout << futils::wrap::pack("failed to writeto: ", res.error().error(), "\n");
            return;
        }

        while (true) {
            futils::fnet::wait_io_event(1);
            if (outer_cb) {
                outer_cb.invoke();
                break;
            }
        }
    }
}

int main() {
    auto result = futils::fnet::connect("localhost", "8080", futils::fnet::sockattr_udp(futils::fnet::ip::Version::ipv4), false);
    if (!result) {
        return 1;
    }
    auto [sock, addr] = std::move(*result);
    single_thread(sock, addr);
    multi_thread(sock, addr);
    return 0;
}