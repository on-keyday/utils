/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// address - address representation
#pragma once
#include "../core/byte.h"
#include "../net_util/ipaddr.h"
#include "../helper/appender.h"
#include "storage.h"

namespace utils {
    namespace dnet {

        enum class NetAddrType : byte {
            null,
            ipv4,
            ipv6,
            unix_path,
            opaque,
        };

        namespace internal {
            constexpr bool NetAddronHeap(NetAddrType type) {
                return type != NetAddrType::ipv4 &&
                       type != NetAddrType::ipv6 &&
                       type != NetAddrType::null;
            }
        }  // namespace internal

        struct NetAddr {
           private:
            friend NetAddr make_netaddr(NetAddrType, view::rvec);
            union {
                byte bdata[16];
                storage fdata;
            };
            NetAddrType type_ = NetAddrType::null;

            constexpr void data_copy(const byte* from, size_t len) {
                for (auto i = 0; i < len; i++) {
                    bdata[i] = from[i];
                }
            }

            void copy(const NetAddr& from) {
                if (internal::NetAddronHeap(from.type_)) {
                    fdata = make_storage(from.fdata);
                    if (!fdata.null()) {
                        type_ = NetAddrType::null;
                        return;
                    }
                }
                else {
                    data_copy(from.bdata, sizeof(bdata));
                }
                type_ = from.type_;
            }

            constexpr void move(NetAddr&& from) {
                if (internal::NetAddronHeap(from.type_)) {
                    fdata = std::move(from.fdata);
                }
                else {
                    data_copy(from.bdata, sizeof(bdata));
                }
                type_ = from.type_;
                from.type_ = NetAddrType::null;
            }

           public:
            constexpr NetAddr()
                : fdata() {}

            ~NetAddr() {
                if (internal::NetAddronHeap(type_)) {
                    fdata.~basic_storage_vec();
                }
            }
            NetAddr(const NetAddr& from)
                : fdata() {
                copy(from);
            }

            constexpr NetAddr(NetAddr&& from)
                : fdata() {
                move(std::move(from));
            }

            NetAddr& operator=(const NetAddr& from) {
                if (this == &from) {
                    return *this;
                }
                this->~NetAddr();
                copy(from);
                return *this;
            }

            constexpr NetAddr& operator=(NetAddr&& from) {
                if (this == &from) {
                    return *this;
                }
                this->~NetAddr();
                move(std::move(from));
                return *this;
            }

            constexpr const byte* data() const {
                if (internal::NetAddronHeap(type_)) {
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
        };

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

        struct NetAddrPort {
            NetAddr addr;
            NetPort port;

            template <class Str>
            constexpr void to_string(Str& str, bool detect_ipv4_mapped = false, bool ipv4mapped_as_ipv4 = false) const {
                if (addr.type() == NetAddrType::ipv4) {
                    ipaddr::ipv4_to_string_with_port(str, addr.data(), port);
                }
                else if (addr.type() == NetAddrType::ipv6) {
                    auto is_v4_mapped = detect_ipv4_mapped && ipaddr::is_ipv4_mapped(addr.data());
                    if (ipv4mapped_as_ipv4 && is_v4_mapped) {
                        ipaddr::ipv4_to_string_with_port(str, addr.data() + 12, port);
                    }
                    else {
                        ipaddr::ipv6_to_string_with_port(str, addr.data(), port, is_v4_mapped);
                    }
                }
                else if (addr.type() == NetAddrType::unix_path) {
                    helper::append(str, (const char*)addr.data());
                }
                else {
                    helper::append(str, "<opaque sockaddr>");
                }
            }

            template <class Str>
            constexpr Str to_string(bool detect_ipv4_mapped = false, bool ipv4mapped_as_ipv4 = false) const {
                Str str;
                to_string(str, detect_ipv4_mapped, ipv4mapped_as_ipv4);
                return str;
            }
        };

        dnet_dll_export(NetAddrPort) ipv4(byte a, byte b, byte c, byte d, std::uint16_t port);
        dnet_dll_export(NetAddrPort) ipv6(byte a, byte b, byte c, byte d, byte e, byte f, byte g, byte h,
                                          byte i, byte j, byte k, byte l, byte m, byte n, byte o, byte p,
                                          std::uint16_t port);

        inline NetAddrPort ipv6(std::uint16_t a, std::uint16_t b, std::uint16_t c, std::uint16_t d,
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

        inline NetAddrPort ipv4(const byte* addr, std::uint16_t port) {
            return ipv4(addr[0], addr[1], addr[2], addr[3], port);
        }

        inline NetAddrPort ipv6(const byte* addr, std::uint16_t port) {
            return ipv6(addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7],
                        addr[8], addr[9], addr[10], addr[11], addr[12], addr[13], addr[14], addr[15], port);
        }

        inline NetAddrPort ipv6(const std::uint16_t* addr, std::uint16_t port) {
            return ipv6(addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7], port);
        }

        std::pair<NetAddr, bool> toipv4(auto&& addr, std::uint16_t port) {
            auto d = ipaddr::to_ipv4(addr);
            if (!d.second) {
                return {{}, false};
            }
            return {ipv4(d.first.addr, port), true};
        }

        std::pair<NetAddr, bool> toipv6(auto&& addr, std::uint16_t port) {
            auto d = ipaddr::to_ipv6(addr);
            if (!d.second) {
                return {{}, false};
            }
            return {ipv4(d.first.addr), true};
        }

        // SockAttr is basic attributes to make socket or search address
        struct SockAttr {
            int address_family = 0;
            int socket_type = 0;
            int protocol = 0;
            int flag = 0;
        };

    }  // namespace dnet
}  // namespace utils
