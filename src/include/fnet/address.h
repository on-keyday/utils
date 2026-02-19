/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// address - address representation
#pragma once
#include "../core/byte.h"
#include "util/ipaddr.h"
#include "../strutil/append.h"
#include "storage.h"
#include "error.h"

namespace futils {
    namespace fnet {

        enum class NetAddrType : byte {
            null,
            ipv4,
            ipv6,
            unix_path,
            link_layer,
            opaque,
        };

        struct NetAddr;

        struct LinkLayerInfoInternal {
            std::uint16_t hardware_type = 0;
            std::uint8_t packet_type = 0;
            std::uint8_t addr_len = 0;
            std::uint32_t ifindex = 0;
            byte addr[8]{};
        };

        struct LinkLayerInfo : LinkLayerInfoInternal {
            std::uint16_t protocol = 0;
        };

        constexpr auto sizeof_link_layer_info = sizeof(LinkLayerInfoInternal);
        static_assert(sizeof_link_layer_info == 16);

        namespace internal {
            constexpr bool NetAddrOnHeap(NetAddrType type) {
                return type != NetAddrType::ipv4 &&
                       type != NetAddrType::ipv6 &&
                       type != NetAddrType::link_layer &&
                       type != NetAddrType::null;
            }
            constexpr NetAddr make_netaddr(NetAddrType type, view::rvec b);
            constexpr NetAddr make_netaddr(NetAddrType type, LinkLayerInfo&& info);

        }  // namespace internal

        struct NetPort {
           private:
            std::uint16_t port = 0;

           public:
            constexpr operator std::uint16_t() const {
                return port;
            }
            constexpr NetPort(std::uint16_t v) {
                port = v;
            }

            constexpr NetPort() = default;

            constexpr std::uint16_t u16() const {
                return port;
            }

            constexpr bool is_system() const {
                return port <= 1023;
            }

            constexpr bool is_user() const {
                return 1024 <= port && port <= 49151;
            }

            constexpr bool is_dynamic_private() const {
                return 49152 <= port;
            }
        };

        // NetAddr is a network address representation
        struct NetAddr {
           private:
            friend constexpr NetAddr internal::make_netaddr(NetAddrType, view::rvec);
            friend constexpr NetAddr internal::make_netaddr(NetAddrType type, LinkLayerInfo&& info);
            union {
                byte bdata[16]{};
                storage fdata;
                LinkLayerInfoInternal link_layer_info;
            };
            NetPort place_for_port_or_protocol = 0;
            NetAddrType type_ = NetAddrType::null;
            friend struct NetAddrPort;

            constexpr void data_copy(const byte* from, size_t len) {
                for (auto i = 0; i < len; i++) {
                    bdata[i] = from[i];
                }
            }

            constexpr void copy(const NetAddr& from) {
                if (internal::NetAddrOnHeap(from.type_)) {
                    fdata = make_storage(from.fdata);
                    if (!fdata.null()) {
                        type_ = NetAddrType::null;
                        return;
                    }
                }
                else {
                    data_copy(from.bdata, from.size());
                }
                place_for_port_or_protocol = from.place_for_port_or_protocol;
                type_ = from.type_;
            }

            constexpr void move(NetAddr&& from) {
                if (internal::NetAddrOnHeap(from.type_)) {
                    fdata = std::move(from.fdata);
                }
                else {
                    data_copy(from.bdata, from.size());
                }
                type_ = from.type_;
                place_for_port_or_protocol = from.place_for_port_or_protocol;
                from.type_ = NetAddrType::null;
                from.place_for_port_or_protocol = 0;
            }

            constexpr void destruct() {
                if (internal::NetAddrOnHeap(type_)) {
                    std::destroy_at(std::addressof(fdata));
                }
            }

           public:
            constexpr NetAddr() {}

            constexpr ~NetAddr() {
                destruct();
            }

            constexpr NetAddr(const NetAddr& from) {
                copy(from);
            }

            constexpr NetAddr(NetAddr&& from) noexcept {
                move(std::move(from));
            }

            constexpr NetAddr& operator=(const NetAddr& from) {
                if (this == &from) {
                    return *this;
                }
                destruct();
                copy(from);
                return *this;
            }

            constexpr NetAddr& operator=(NetAddr&& from) noexcept {
                if (this == &from) {
                    return *this;
                }
                destruct();
                move(std::move(from));
                return *this;
            }

            constexpr const byte* data() const {
                if (internal::NetAddrOnHeap(type_)) {
                    return fdata.cdata();
                }
                return const_cast<byte*>(bdata);
            }

            constexpr size_t size() const {
                if (type_ == NetAddrType::null) {
                    return 0;
                }
                if (type_ == NetAddrType::ipv4) {
                    return 4;
                }
                if (type_ == NetAddrType::ipv6) {
                    return 16;
                }
                return fdata.size();
            }

            constexpr NetAddrType type() const {
                return type_;
            }

            template <class Str>
            constexpr void to_string(Str& str, bool detect_ipv4_mapped = false, bool ipv4mapped_as_ipv4 = false) const {
                if (type() == NetAddrType::null) {
                    strutil::append(str, "<null sockaddr>");
                }
                else if (type() == NetAddrType::ipv4) {
                    ipaddr::ipv4_to_string(str, data());
                }
                else if (type() == NetAddrType::ipv6) {
                    auto is_v4_mapped = detect_ipv4_mapped && ipaddr::is_ipv4_mapped(data());
                    if (ipv4mapped_as_ipv4 && is_v4_mapped) {
                        ipaddr::ipv4_to_string(str, data() + 12);
                    }
                    else {
                        ipaddr::ipv6_to_string(str, data(), is_v4_mapped);
                    }
                }
                else if (type() == NetAddrType::unix_path) {
                    strutil::append(str, (const char*)data());
                }
                else {
                    strutil::append(str, "<opaque sockaddr>");
                }
            }

            template <class Str>
            constexpr Str to_string(bool detect_ipv4_mapped = false, bool ipv4mapped_as_ipv4 = false) const {
                Str str;
                to_string(str, detect_ipv4_mapped, ipv4mapped_as_ipv4);
                return str;
            }

            constexpr bool is_loopback() const {
                switch (type_) {
                    case NetAddrType::ipv4:
                        return bdata[0] == 127;
                    case NetAddrType::ipv6:
                        return bdata[0] == 0 && bdata[1] == 0 && bdata[2] == 0 && bdata[3] == 0 &&
                               bdata[4] == 0 && bdata[5] == 0 && bdata[6] == 0 && bdata[7] == 0 &&
                               bdata[8] == 0 && bdata[9] == 0 && bdata[10] == 0 && bdata[11] == 0 &&
                               bdata[12] == 0 && bdata[13] == 0 && bdata[14] == 0 && bdata[15] == 1;
                    default:
                        return false;
                }
            }
        };

        constexpr auto sizeof_NetAddr = sizeof(NetAddr);

        struct NetAddrPort {
            NetAddr addr;
            constexpr NetPort& port() noexcept {
                return addr.place_for_port_or_protocol;
            }

            constexpr const NetPort& port() const noexcept {
                return addr.place_for_port_or_protocol;
            }

            constexpr void port(NetPort p) noexcept {
                addr.place_for_port_or_protocol = p;
            }

            template <class Str>
            constexpr void to_string(Str& str, bool detect_ipv4_mapped = false, bool ipv4mapped_as_ipv4 = false) const {
                if (addr.type() == NetAddrType::ipv4) {
                    ipaddr::ipv4_to_string_with_port(str, addr.data(), port());
                }
                else if (addr.type() == NetAddrType::ipv6) {
                    auto is_v4_mapped = detect_ipv4_mapped && ipaddr::is_ipv4_mapped(addr.data());
                    if (ipv4mapped_as_ipv4 && is_v4_mapped) {
                        ipaddr::ipv4_to_string_with_port(str, addr.data() + 12, port());
                    }
                    else {
                        ipaddr::ipv6_to_string_with_port(str, addr.data(), port(), is_v4_mapped);
                    }
                }
                else if (addr.type() == NetAddrType::unix_path) {
                    strutil::append(str, (const char*)addr.data());
                }
                else {
                    strutil::append(str, "<opaque sockaddr>");
                }
            }

            template <class Str>
            constexpr Str to_string(bool detect_ipv4_mapped = false, bool ipv4mapped_as_ipv4 = false) const {
                Str str;
                to_string(str, detect_ipv4_mapped, ipv4mapped_as_ipv4);
                return str;
            }
        };

        namespace internal {
            constexpr NetAddr make_netaddr(NetAddrType type, view::rvec b) {
                NetAddr addr;
                if (internal::NetAddrOnHeap(type)) {
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

            constexpr NetAddr make_netaddr(NetAddrType type, LinkLayerInfo&& info) {
                NetAddr addr;
                addr.link_layer_info = static_cast<LinkLayerInfoInternal&&>(info);
                addr.place_for_port_or_protocol = info.protocol;
                addr.type_ = type;
                return addr;
            }
        }  // namespace internal

        constexpr NetAddrPort ipv4(byte a, byte b, byte c, byte d,
                                   std::uint16_t port) {
            byte val[] = {a, b, c, d};
            NetAddrPort naddr;
            naddr.addr = internal::make_netaddr(NetAddrType::ipv4, val);
            naddr.port() = port;
            return naddr;
        }

        constexpr NetAddrPort ipv6(byte a, byte b, byte c, byte d, byte e, byte f, byte g, byte h,
                                   byte i, byte j, byte k, byte l, byte m, byte n, byte o, byte p,
                                   std::uint16_t port) {
            byte val[] = {a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p};
            NetAddrPort naddr;
            naddr.addr = internal::make_netaddr(NetAddrType::ipv6, val);
            naddr.port() = port;
            return naddr;
        }

        constexpr NetAddrPort ipv6(std::uint16_t a, std::uint16_t b, std::uint16_t c, std::uint16_t d,
                                   std::uint16_t e, std::uint16_t f, std::uint16_t g, std::uint16_t h,
                                   std::uint16_t port) {
            constexpr auto h_ = [](std::uint16_t b) {
                return byte((b >> 8) & 0xff);
            };
            constexpr auto l_ = [](std::uint16_t b) {
                return byte(b & 0xff);
            };
            return ipv6(h_(a), l_(a), h_(b), l_(b), h_(c), l_(c), h_(d), l_(d),
                        h_(e), l_(e), h_(f), l_(f), h_(g), l_(g), h_(h), l_(h),
                        port);
        }

        constexpr NetAddrPort ipv4(const byte* addr, std::uint16_t port) {
            return ipv4(addr[0], addr[1], addr[2], addr[3], port);
        }

        constexpr NetAddrPort ipv6(const byte* addr, std::uint16_t port) {
            return ipv6(addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7],
                        addr[8], addr[9], addr[10], addr[11], addr[12], addr[13], addr[14], addr[15], port);
        }

        inline NetAddrPort ipv6(const std::uint16_t* addr, std::uint16_t port) {
            return ipv6(addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7], port);
        }

        constexpr expected<NetAddrPort> to_ipv4(auto&& addr, std::uint16_t port) {
            auto d = ipaddr::to_ipv4(addr);
            if (!d.second) {
                return unexpect("not valid IPv4 address", error::Category::lib, error::fnet_address_error);
            }
            return ipv4(d.first.addr, port);
        }

        constexpr expected<NetAddrPort> to_ipv6(auto&& addr, std::uint16_t port, bool ipv4map = false) {
            auto d = ipaddr::to_ipv6(addr, ipv4map);
            if (!d.second) {
                return unexpect("not valid IPv6 address", error::Category::lib, error::fnet_address_error);
            }
            return ipv6(d.first.addr, port);
        }

        constexpr NetAddrPort link_layer(LinkLayerInfo&& info) {
            NetAddrPort naddr;
            naddr.addr = internal::make_netaddr(NetAddrType::link_layer, static_cast<LinkLayerInfo&&>(info));
            return naddr;
        }

        constexpr auto ipv4_any = ipv4(0, 0, 0, 0, 0);
        constexpr auto ipv6_any = ipv6(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

        // SockAttr is basic attributes to make socket or search address
        struct SockAttr {
            int address_family = 0;
            int socket_type = 0;
            int protocol = 0;
            int flag = 0;
        };

    }  // namespace fnet
}  // namespace futils
