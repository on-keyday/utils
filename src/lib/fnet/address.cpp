/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/dll/dllcpp.h>
#include <fnet/address.h>
#include <fnet/plthead.h>
#include <cstring>
#include <binary/buf.h>

namespace futils {
    namespace fnet {

        NetAddrPort sockaddr_to_NetAddrPort(sockaddr* addr, size_t len) {
            NetAddrPort naddr;
            if (addr->sa_family == AF_INET) {
                auto p = reinterpret_cast<sockaddr_in*>(addr);
                binary::Buf<std::uint32_t> buf;
                buf.write_be(binary::bswap_net(p->sin_addr.s_addr));
                naddr.addr = internal::make_netaddr(NetAddrType::ipv4, buf.data_);
                naddr.port() = binary::bswap_net(p->sin_port);
            }
            else if (addr->sa_family == AF_INET6) {
                auto p = reinterpret_cast<sockaddr_in6*>(addr);
                naddr.addr = internal::make_netaddr(NetAddrType::ipv6, p->sin6_addr.s6_addr);
                naddr.port() = binary::bswap_net(p->sin6_port);
            }
            else if (addr->sa_family == AF_UNIX) {
                auto p = reinterpret_cast<sockaddr_un*>(addr);
                auto len = futils::strlen(p->sun_path) + 1;
                naddr.addr = internal::make_netaddr(NetAddrType::unix_path, view::rvec(p->sun_path, len));
                naddr.port() = {};
            }
            else if (addr->sa_family == AF_UNSPEC) {
                // nothing to do
            }
            else {
                auto opaque = reinterpret_cast<byte*>(addr);
                naddr.addr = internal::make_netaddr(NetAddrType::opaque, view::rvec(opaque, len));
            }
            return naddr;
        }

        std::pair<sockaddr*, int> NetAddrPort_to_sockaddr(sockaddr_storage* addr, const NetAddrPort& naddr) {
            int addrlen = 0;
            if (naddr.addr.type() == NetAddrType::ipv4) {
                auto p = reinterpret_cast<sockaddr_in*>(addr);
                *p = {};
                p->sin_family = AF_INET;
                binary::Buf<std::uint32_t, const byte*> buf{naddr.addr.data()};
                p->sin_addr.s_addr = binary::bswap_net(buf.read_be());
                p->sin_port = binary::bswap_net(naddr.port().u16());
                addrlen = sizeof(sockaddr_in);
            }
            else if (naddr.addr.type() == NetAddrType::ipv6) {
                auto p = reinterpret_cast<sockaddr_in6*>(addr);
                *p = {};
                p->sin6_family = AF_INET6;
                memcpy(p->sin6_addr.s6_addr, naddr.addr.data(), 16);
                p->sin6_port = binary::bswap_net(naddr.port().u16());
                addrlen = sizeof(sockaddr_in6);
            }
            else if (naddr.addr.type() == NetAddrType::unix_path) {
                auto p = reinterpret_cast<sockaddr_un*>(addr);
                *p = {};
                p->sun_family = AF_UNIX;
                if (sizeof(p->sun_path) <= naddr.addr.size()) {
                    return {nullptr, 0};
                }
                memcpy(p->sun_path, naddr.addr.data(), naddr.addr.size());
                addrlen = sizeof(sockaddr_un);
            }
#ifdef FUTILS_PLATFORM_LINUX
            else if (naddr.addr.type() == NetAddrType::link_layer) {
                auto p = reinterpret_cast<sockaddr_ll*>(addr);
                *p = {};
                p->sll_family = AF_PACKET;
                p->sll_protocol = naddr.port().u16();
                p->sll_ifindex = naddr.addr.size();
                addrlen = sizeof(sockaddr_ll);
            }
#endif
            else {
                return {nullptr, 0};
            }
            return {reinterpret_cast<sockaddr*>(addr), addrlen};
        }
    }  // namespace fnet
}  // namespace futils
