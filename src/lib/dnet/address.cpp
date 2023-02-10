/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/dllcpp.h>
#include <dnet/address.h>
#include <dnet/plthead.h>
#include <dnet/dll/asyncbase.h>
#include <cstring>
#include <endian/buf.h>

namespace utils {
    namespace dnet {
        NetAddr make_netaddr(NetAddrType type, view::rvec b) {
            NetAddr addr;
            if (internal::NetAddronHeap(type)) {
                addr.fdata = make_storage(b);
                if (addr.fdata.null()) {
                    return {};
                }
            }
            else {
                addr.data_copy(b.data(), b.size());
            }
            addr.type_ = type;
            return addr;
        }

        dnet_dll_implement(NetAddrPort) ipv4(byte a, byte b, byte c, byte d,
                                             std::uint16_t port) {
            byte val[] = {a, b, c, d};
            NetAddrPort naddr;
            naddr.addr = make_netaddr(NetAddrType::ipv4, val);
            naddr.port = port;
            return naddr;
        }

        dnet_dll_implement(NetAddrPort) ipv6(byte a, byte b, byte c, byte d, byte e, byte f, byte g, byte h,
                                             byte i, byte j, byte k, byte l, byte m, byte n, byte o, byte p,
                                             std::uint16_t port) {
            byte val[] = {a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p};
            NetAddrPort naddr;
            naddr.addr = make_netaddr(NetAddrType::ipv6, val);
            naddr.port = port;
            return naddr;
        }

        NetAddrPort sockaddr_to_NetAddrPort(sockaddr* addr, size_t len) {
            NetAddrPort naddr;
            if (addr->sa_family == AF_INET) {
                auto p = reinterpret_cast<sockaddr_in*>(addr);
                endian::Buf<std::uint32_t> buf;
                buf.write_be(endian::bswap_net(p->sin_addr.s_addr));
                naddr.addr = make_netaddr(NetAddrType::ipv4, buf.data);
                naddr.port = endian::bswap_net(p->sin_port);
            }
            else if (addr->sa_family == AF_INET6) {
                auto p = reinterpret_cast<sockaddr_in6*>(addr);
                naddr.addr = make_netaddr(NetAddrType::ipv6, p->sin6_addr.s6_addr);
                naddr.port = endian::bswap_net(p->sin6_port);
            }
            else if (addr->sa_family == AF_UNIX) {
                auto p = reinterpret_cast<sockaddr_un*>(addr);
                auto len = utils::strlen(p->sun_path) + 1;
                naddr.addr = make_netaddr(NetAddrType::unix_path, view::rvec(p->sun_path, len));
                naddr.port = {};
            }
            else if (addr->sa_family == AF_UNSPEC) {
                // nothing to do
            }
            else {
                auto opaque = reinterpret_cast<byte*>(addr);
                naddr.addr = make_netaddr(NetAddrType::opaque, view::rvec(opaque, len));
            }
            return naddr;
        }

        std::pair<sockaddr*, int> NetAddrPort_to_sockaddr(sockaddr_storage* addr, const NetAddrPort& naddr) {
            int addrlen = 0;
            if (naddr.addr.type() == NetAddrType::ipv4) {
                auto p = reinterpret_cast<sockaddr_in*>(addr);
                *p = {};
                p->sin_family = AF_INET;
                endian::Buf<std::uint32_t, const byte*> buf{naddr.addr.data()};
                p->sin_addr.s_addr = buf.read_be();
                p->sin_port = endian::bswap_net(naddr.port.u16());
                addrlen = sizeof(sockaddr_in);
            }
            else if (naddr.addr.type() == NetAddrType::ipv6) {
                auto p = reinterpret_cast<sockaddr_in6*>(addr);
                *p = {};
                p->sin6_family = AF_INET6;
                memcpy(p->sin6_addr.s6_addr, naddr.addr.data(), 16);
                p->sin6_port = endian::bswap_net(naddr.port.u16());
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
            else {
                return {nullptr, 0};
            }
            return {reinterpret_cast<sockaddr*>(addr), addrlen};
        }
    }  // namespace dnet
}  // namespace utils
