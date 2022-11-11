/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/dllcpp.h>
#include <dnet/address.h>
#include <dnet/plthead.h>
#include <dnet/dll/asyncbase.h>
#include <cstring>

namespace utils {
    namespace dnet {
        NetAddr make_netaddr(NetAddrType type, ByteLen b) {
            NetAddr addr;
            if (internal::NetAddronHeap(type)) {
                addr.fdata = b;
                if (!addr.fdata.valid()) {
                    return {};
                }
            }
            else {
                addr.data_copy(b.data, b.len);
            }
            return addr;
        }

        NetAddrPort sockaddr_to_NetAddrPort(sockaddr* addr, size_t len) {
            NetAddrPort naddr;
            if (addr->sa_family == AF_INET) {
                auto p = reinterpret_cast<sockaddr_in*>(addr);
                byte data[4];
                WPacket w{{data, 4}};
                auto norder = netorder(p->sin_addr.s_addr);
                w.as(norder);
                naddr.addr = make_netaddr(NetAddrType::ipv4, ByteLen{data, 4});
                naddr.port = netorder(p->sin_port);
            }
            else if (addr->sa_family == AF_INET6) {
                auto p = reinterpret_cast<sockaddr_in6*>(addr);
                byte data[16];
                WPacket w{{data, 16}};
                w.copy_from(ByteLen{p->sin6_addr.s6_addr, 16});
                naddr.addr = make_netaddr(NetAddrType::ipv6, ByteLen{data, 16});
                naddr.port = netorder(p->sin6_port);
            }
            else if (addr->sa_family == AF_UNIX) {
                auto p = reinterpret_cast<sockaddr_un*>(addr);
                auto len = utils::strlen(p->sun_path) + 1;
                naddr.addr = make_netaddr(NetAddrType::unix_path, ByteLen{(byte*)p->sun_path, len});
                naddr.port = {};
            }
            else {
                auto opaque = reinterpret_cast<byte*>(addr);
                naddr.addr = make_netaddr(NetAddrType::opaque, ByteLen{opaque, len});
            }
            return naddr;
        }

        std::pair<sockaddr*, int> NetAddrPort_to_sockaddr(sockaddr_storage* addr, const NetAddrPort& naddr) {
            int addrlen = 0;
            if (naddr.addr.type() == NetAddrType::ipv4) {
                auto p = reinterpret_cast<sockaddr_in*>(addr);
                *p = {};
                p->sin_family = AF_INET;
                p->sin_addr.s_addr = ConstByteLen{naddr.addr.data(), naddr.addr.size()}.as_netorder<std::uint32_t>();
                p->sin_port = ConstByteLen{naddr.port.data(), 2}.as_netorder<std::uint16_t>();
                addrlen = sizeof(sockaddr_in);
            }
            else if (naddr.addr.type() == NetAddrType::ipv6) {
                auto p = reinterpret_cast<sockaddr_in6*>(addr);
                *p = {};
                p->sin6_family = AF_INET6;
                memcpy(p->sin6_addr.s6_addr, naddr.addr.data(), 16);
                p->sin6_port = ConstByteLen{naddr.port.data(), 2}.as_netorder<std::uint16_t>();
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
