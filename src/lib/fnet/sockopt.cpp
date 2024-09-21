/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/dll/dllcpp.h>
#include <fnet/socket.h>
#include <fnet/plthead.h>
#include <fnet/dll/lazy/sockdll.h>
#include <fnet/sock_internal.h>
#include <platform/detect.h>

namespace futils {
    namespace fnet {

        expected<std::uintptr_t> Socket::get_raw() {
            if (!ctx) {
                return unexpect("socket not initialized", error::Category::lib, error::fnet_usage_error);
            }
            return static_cast<SockTable*>(ctx)->sock;
        }

        void decr_table(void* tbl) {
            if (tbl) {
                static_cast<SockTable*>(tbl)->decr();
            }
        }

        Socket Socket::clone() const {
            if (!ctx) {
                return Socket();
            }
            static_cast<SockTable*>(ctx)->incr();  // increment for clone
            return make_socket(ctx);
        };

        expected<NetAddrPort> Socket::get_local_addr() {
            return get_raw().and_then([&](std::uintptr_t sock) -> expected<NetAddrPort> {
                sockaddr_storage st{};
                socklen_t len = sizeof(st);
                auto addr = reinterpret_cast<sockaddr*>(&st);
                auto res = lazy::getsockname_(sock, addr, &len);
                if (res != 0) {
                    return unexpect(error::Errno());
                }
                return sockaddr_to_NetAddrPort(addr, len);
            });
        }

        expected<NetAddrPort> Socket::get_remote_addr() {
            return get_raw().and_then([&](std::uintptr_t sock) -> expected<NetAddrPort> {
                sockaddr_storage st{};
                socklen_t len = sizeof(st);
                auto addr = reinterpret_cast<sockaddr*>(&st);
                auto res = lazy::getpeername_(sock, addr, &len);
                if (res != 0) {
                    return unexpect(error::Errno());
                }
                return sockaddr_to_NetAddrPort(addr, len);
            });
        }

        expected<void> Socket::get_option(int layer, int opt, void* buf, size_t size) {
            return get_raw().and_then([&](std::uintptr_t sock) -> expected<void> {
                socklen_t len = int(size);
                auto res = lazy::getsockopt_(sock, layer, opt, static_cast<char*>(buf), &len);
                if (res != 0) {
                    return unexpect(error::Errno());
                }
                return {};
            });
        }

        expected<void> Socket::set_option(int layer, int opt, const void* buf, size_t size) {
            return get_raw().and_then([&](std::uintptr_t sock) -> expected<void> {
                auto res = lazy::setsockopt_(sock, layer, opt, static_cast<const char*>(buf), int(size));
                if (res != 0) {
                    return unexpect(error::Errno());
                }
                return {};
            });
        }

        expected<void> Socket::set_reuse_addr(bool resue) {
            int yes = resue ? 1 : 0;
            return set_option(SOL_SOCKET, SO_REUSEADDR, yes);
        }

        expected<void> Socket::set_ipv6only(bool only) {
            int yes = only ? 1 : 0;
            return set_option(IPPROTO_IPV6, IPV6_V6ONLY, yes);
        }

        expected<void> Socket::set_nodelay(bool no_delay) {
            int yes = no_delay ? 1 : 0;
            return set_option(IPPROTO_TCP, TCP_NODELAY, yes);
        }

        expected<void> Socket::set_ttl(unsigned char ttl) {
            int ttl_buf = ttl;
            return set_option(IPPROTO_IP, IP_TTL, ttl_buf);
        }

        expected<void> Socket::set_exclusive_use(bool exclusive) {
#ifdef FUTILS_PLATFORM_WINDOWS
            int yes = exclusive ? 1 : 0;
            return set_option(SOL_SOCKET, SO_EXCLUSIVEADDRUSE, yes);
#else
            return unexpect(error::Error("SO_EXCLUSIVEADDRUSE is not supported on linux", error::Category::lib, error::fnet_usage_error));
#endif
        }

        expected<void> Socket::set_mtu_discover(MTUConfig conf) {
            int val = 0;
            if (conf == mtu_default) {
#ifdef FUTILS_PLATFORM_WINDOWS
                val = IP_PMTUDISC_NOT_SET;
#else
                val = IP_PMTUDISC_WANT;
#endif
            }
            else if (conf == mtu_enable) {
                val = IP_PMTUDISC_DO;
            }
            else if (conf == mtu_disable) {
                val = IP_PMTUDISC_DONT;
            }
            else if (conf == mtu_ignore) {
                val = IP_PMTUDISC_PROBE;
            }
            else {
                return unexpect(error::Error("invalid MTUConfig", error::Category::lib, error::fnet_usage_error));
            }
            return set_option(IPPROTO_IP, IP_MTU_DISCOVER, val);
        }

        expected<void> Socket::set_mtu_discover_v6(MTUConfig conf) {
            int val = 0;
            if (conf == mtu_default) {
#ifdef FUTILS_PLATFORM_WINDOWS
                val = IP_PMTUDISC_NOT_SET;
#else
                val = IP_PMTUDISC_WANT;
#endif
            }
            else if (conf == mtu_enable) {
                val = IP_PMTUDISC_DO;
            }
            else if (conf == mtu_disable) {
                val = IP_PMTUDISC_DONT;
            }
            else if (conf == mtu_ignore) {
                val = IP_PMTUDISC_PROBE;
            }
            else {
                return unexpect(error::Error("invalid MTUConfig", error::Category::lib, error::fnet_usage_error));
            }
            return set_option(IPPROTO_IPV6, IPV6_MTU_DISCOVER, val);
        }

        expected<std::int32_t> Socket::get_mtu() {
            std::int32_t val = 0;
            return get_option(IPPROTO_IP, IP_MTU, val).transform([&] { return val; });
        }

        void Socket::set_blocking(bool blocking) {
            get_raw().transform([&](std::uintptr_t sock) {
                set_nonblock(sock, !blocking);
            });
        }

        expected<void> Socket::set_dont_fragment_v4(bool df) {
#ifdef FUTILS_PLATFORM_WINDOWS
            return set_option(IPPROTO_IP, IP_DONTFRAGMENT, std::uint32_t(df ? 1 : 0));
#else
            return unexpect(error::Error("IP_DONTFRAGMENT is not supported on linux", error::Category::lib, error::fnet_usage_error));
#endif
        }

        expected<void> Socket::set_dont_fragment_v6(bool df) {
            return set_option(IPPROTO_IP, IPV6_DONTFRAG, std::uint32_t(df ? 1 : 0));
        }

        expected<void> Socket::set_connreset(bool enable) {
#ifdef FUTILS_PLATFORM_WINDOWS
            return get_raw().and_then([&](std::uintptr_t sock) -> expected<void> {
                std::int32_t flag = enable ? 1 : 0;
                DWORD ret = 0;
                auto err = lazy::WSAIoctl_(sock, SIO_UDP_CONNRESET, &flag, sizeof(flag), nullptr, 0, &ret, nullptr, nullptr);
                if (err == SOCKET_ERROR) {
                    return unexpect(error::Errno());
                }
                return {};
            });
#else
            return unexpect(error::Error("SIO_UDP_CONNRESET is not supported", error::Category::lib, error::fnet_usage_error));
#endif
        }

        expected<SockAttr> Socket::get_sockattr() {
#ifdef FUTILS_PLATFORM_WINDOWS
            WSAPROTOCOL_INFOW info{};
            return get_option(SOL_SOCKET, SO_PROTOCOL_INFOW, info).transform([&] {
                return SockAttr{
                    .address_family = info.iAddressFamily,
                    .socket_type = info.iSocketType,
                    .protocol = info.iProtocol,
                };
            });
#else
            SockAttr attr{-1, -1, -1};
            return get_option(SOL_SOCKET, SO_DOMAIN, attr.address_family)
                .and_then([&] {
                    return get_option(SOL_SOCKET, SO_TYPE, attr.socket_type);
                })
                .and_then([&] {
                    return get_option(SOL_SOCKET, SO_PROTOCOL, attr.protocol);
                })
                .transform([&] { return attr; });
#endif
        }

        expected<void> Socket::set_DF(bool df) {
#ifdef FUTILS_PLATFORM_WINDOWS
            auto errv4 = set_dont_fragment_v6(df);
            auto errv6 = set_dont_fragment_v4(df);
#else
            auto errv4 = set_mtu_discover(mtu_enable);
            auto errv6 = set_mtu_discover_v6(mtu_enable);
#endif
            if (!errv4 || !errv6) {
                if (!errv4 && !errv6) {
                    return unexpect(SetDFError{std::move(errv4.error()), std::move(errv6.error())});
                }
                return !errv4 ? errv4 : errv6;
            }
            return {};
        }

        // for raw socket
        expected<void> Socket::set_header_include_v4(bool incl) {
            int on = incl ? 1 : 0;
            return set_option(IPPROTO_IP, IP_HDRINCL, on);
        }

        expected<void> Socket::set_header_include_v6(bool incl) {
            int on = incl ? 1 : 0;
            return set_option(IPPROTO_IPV6, IPV6_HDRINCL, on);
        }

        expected<void> Socket::set_send_buffer_size(size_t size) {
            return set_option(SOL_SOCKET, SO_SNDBUF, int(size));
        }
        expected<void> Socket::set_recv_buffer_size(size_t size) {
            return set_option(SOL_SOCKET, SO_RCVBUF, int(size));
        }

        expected<size_t> Socket::get_send_buffer_size() {
            int size = 0;
            return get_option(SOL_SOCKET, SO_SNDBUF, size).transform([&] { return size_t(size); });
        }

        expected<size_t> Socket::get_recv_buffer_size() {
            int size = 0;
            return get_option(SOL_SOCKET, SO_RCVBUF, size).transform([&] { return size_t(size); });
        }

        expected<void> Socket::set_gso(bool enable) {
#ifdef FUTILS_PLATFORM_LINUX
            int val = enable ? 1 : 0;
            return set_option(SOL_UDP, UDP_SEGMENT, val);
#else
            return unexpect(error::Error("GSO is not supported", error::Category::lib, error::fnet_usage_error));
#endif
        }

        expected<bool> Socket::get_gso() {
#ifdef FUTILS_PLATFORM_LINUX
            int val = 0;
            return get_option(SOL_UDP, UDP_SEGMENT, val).transform([&] { return val != 0; });
#else
            return unexpect(error::Error("GSO is not supported", error::Category::lib, error::fnet_usage_error));
#endif
        }

        expected<void> Socket::set_recv_packet_info_v4(bool enable) {
#ifdef FUTILS_PLATFORM_LINUX
            int val = enable ? 1 : 0;
            return set_option(IPPROTO_IP, IP_RECVPKTINFO, val);
#else
            return unexpect(error::Error("IP_RECVPKTINFO is not supported", error::Category::lib, error::fnet_usage_error));
#endif
        }

        expected<bool> Socket::get_recv_packet_info_v4() {
#ifdef FUTILS_PLATFORM_LINUX
            int val = 0;
            return get_option(IPPROTO_IP, IP_RECVPKTINFO, val).transform([&] { return val != 0; });
#else
            return unexpect(error::Error("IP_RECVPKTINFO is not supported", error::Category::lib, error::fnet_usage_error));
#endif
        }

        expected<void> Socket::set_recv_packet_info_v6(bool enable) {
#ifdef FUTILS_PLATFORM_LINUX
            int val = enable ? 1 : 0;
            return set_option(IPPROTO_IPV6, IPV6_RECVPKTINFO, val);
#else
            return unexpect(error::Error("IPV6_RECVPKTINFO is not supported", error::Category::lib, error::fnet_usage_error));
#endif
        }

        expected<bool> Socket::get_recv_packet_info_v6() {
#ifdef FUTILS_PLATFORM_LINUX
            int val = 0;
            return get_option(IPPROTO_IPV6, IPV6_RECVPKTINFO, val).transform([&] { return val != 0; });
#else
            return unexpect(error::Error("IPV6_RECVPKTINFO is not supported", error::Category::lib, error::fnet_usage_error));
#endif
        }

        expected<void> Socket::set_recv_tos(bool enable) {
#ifdef FUTILS_PLATFORM_LINUX
            int val = enable ? 1 : 0;
            return set_option(IPPROTO_IP, IP_RECVTOS, val);
#else
            return unexpect(error::Error("IP_RECVTOS is not supported", error::Category::lib, error::fnet_usage_error));
#endif
        }

        expected<bool> Socket::get_recv_tos() {
#ifdef FUTILS_PLATFORM_LINUX
            int val = 0;
            return get_option(IPPROTO_IP, IP_RECVTOS, val).transform([&] { return val != 0; });
#else
            return unexpect(error::Error("IP_RECVTOS is not supported", error::Category::lib, error::fnet_usage_error));
#endif
        }

        expected<void> Socket::set_recv_tclass(bool enable) {
#ifdef FUTILS_PLATFORM_LINUX
            int val = enable ? 1 : 0;
            return set_option(IPPROTO_IPV6, IPV6_RECVTCLASS, val);
#else
            return unexpect(error::Error("IPV6_RECVTCLASS is not supported", error::Category::lib, error::fnet_usage_error));
#endif
        }

        expected<bool> Socket::get_recv_tclass() {
#ifdef FUTILS_PLATFORM_LINUX
            int val = 0;
            return get_option(IPPROTO_IPV6, IPV6_RECVTCLASS, val).transform([&] { return val != 0; });
#else
            return unexpect(error::Error("IPV6_RECVTCLASS is not supported", error::Category::lib, error::fnet_usage_error));
#endif
        }
    }  // namespace fnet
}  // namespace futils
