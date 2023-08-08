/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/dll/dllcpp.h>
#include <fnet/socket.h>
#include <fnet/plthead.h>
#include <fnet/dll/asyncbase.h>
#include <fnet/dll/lazy/sockdll.h>

namespace utils {
    namespace fnet {

        std::pair<NetAddrPort, error::Error> Socket::get_localaddr() {
            sockaddr_storage st{};
            socklen_t len = sizeof(st);
            auto addr = reinterpret_cast<sockaddr*>(&st);
            auto res = lazy::getsockname_(sock, addr, &len);
            if (res != 0) {
                return {{}, error::Errno()};
            }
            return {sockaddr_to_NetAddrPort(addr, len), error::none};
        }

        std::pair<NetAddrPort, error::Error> Socket::get_remoteaddr() {
            sockaddr_storage st{};
            socklen_t len = sizeof(st);
            auto addr = reinterpret_cast<sockaddr*>(&st);
            auto res = lazy::getpeername_(sock, addr, &len);
            if (res != 0) {
                return {{}, error::Errno()};
            }
            return {sockaddr_to_NetAddrPort(addr, len), error::none};
        }

        error::Error Socket::get_option(int layer, int opt, void* buf, size_t size) {
            socklen_t len = int(size);
            auto res = lazy::getsockopt_(sock, layer, opt, static_cast<char*>(buf), &len);
            if (res != 0) {
                return error::Errno();
            }
            return error::none;
        }

        error::Error Socket::set_option(int layer, int opt, const void* buf, size_t size) {
            auto res = lazy::setsockopt_(sock, layer, opt, static_cast<const char*>(buf), int(size));
            if (res != 0) {
                return error::Errno();
            }
            return error::none;
        }

        error::Error Socket::set_reuse_addr(bool resue) {
            int yes = resue ? 1 : 0;
            return set_option(SOL_SOCKET, SO_REUSEADDR, yes);
        }

        error::Error Socket::set_ipv6only(bool only) {
            int yes = only ? 1 : 0;
            return set_option(IPPROTO_IPV6, IPV6_V6ONLY, yes);
        }

        error::Error Socket::set_nodelay(bool no_delay) {
            int yes = no_delay ? 1 : 0;
            return set_option(IPPROTO_TCP, TCP_NODELAY, yes);
        }

        error::Error Socket::set_ttl(unsigned char ttl) {
            int ttl_buf = ttl;
            return set_option(IPPROTO_IP, IP_TTL, ttl_buf);
        }

        error::Error Socket::set_exclusive_use(bool exclusive) {
#ifdef _WIN32
            int yes = exclusive ? 1 : 0;
            return set_option(SOL_SOCKET, SO_EXCLUSIVEADDRUSE, yes);
#else
            return error::Error("SO_EXCLUSIVEADDRUSE is not supported on linux", error::ErrorCategory::fneterr);
#endif
        }

        error::Error Socket::set_mtu_discover(MTUConfig conf) {
            int val = 0;
            if (conf == mtu_default) {
#ifdef _WIN32
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
                return error::Error("invalid MTUConfig", error::ErrorCategory::validationerr);
            }
            return set_option(IPPROTO_IP, IP_MTU_DISCOVER, val);
        }

        error::Error Socket::set_mtu_discover_v6(MTUConfig conf) {
            int val = 0;
            if (conf == mtu_default) {
#ifdef _WIN32
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
                return error::Error("invalid MTUConfig", error::ErrorCategory::validationerr);
            }
            return set_option(IPPROTO_IPV6, IPV6_MTU_DISCOVER, val);
        }

        std::int32_t Socket::get_mtu() {
            std::int32_t val = 0;
            if (!get_option(IPPROTO_IP, IP_MTU, val)) {
                return -1;
            }
            return val;
        }

        void Socket::set_blocking(bool blocking) {
            set_nonblock(sock, !blocking);
        }

        error::Error Socket::set_dontfragment(bool df) {
#ifdef _WIN32
            return set_option(IPPROTO_IP, IP_DONTFRAGMENT, std::uint32_t(df ? 1 : 0));
#else
            return error::Error("IP_DONTFRAGMENT is not supported on linux", error::ErrorCategory::fneterr);
#endif
        }

        error::Error Socket::set_dontfragment_v6(bool df) {
            return set_option(IPPROTO_IP, IPV6_DONTFRAG, std::uint32_t(df ? 1 : 0));
        }

        error::Error Socket::set_connreset(bool enable) {
#ifdef _WIN32
            std::int32_t flag = enable ? 1 : 0;
            DWORD ret = 0;
            auto err = lazy::WSAIoctl_(sock, SIO_UDP_CONNRESET, &flag, sizeof(flag), nullptr, 0, &ret, nullptr, nullptr);
            if (err == SOCKET_ERROR) {
                return error::Errno();
            }
            return error::none;
#else
            return error::Error("SIO_UDP_CONNRESET is not supported", error::ErrorCategory::fneterr);
#endif
        }

        std::pair<SockAttr, error::Error> Socket::get_sockattr() {
#ifdef _WIN32
            WSAPROTOCOL_INFOW info{};
            auto err = get_option(SOL_SOCKET, SO_PROTOCOL_INFOW, info);
            if (err) {
                return {{-1, -1, -1}, err};
            }
            return {
                SockAttr{
                    .address_family = info.iAddressFamily,
                    .socket_type = info.iSocketType,
                    .protocol = info.iProtocol,
                },
                error::none,
            };
#else
            SockAttr attr{-1, -1, -1};
            auto err = get_option(SOL_SOCKET, SO_DOMAIN, attr.address_family);
            if (err) {
                return {attr, err};
            }
            err = get_option(SOL_SOCKET, SO_TYPE, attr.socket_type);
            if (err) {
                return {attr, err};
            }
            err = get_option(SOL_SOCKET, SO_PROTOCOL, attr.protocol);
            if (err) {
                return {attr, err};
            }
            return {attr, error::none};
#endif
        }

        error::Error Socket::set_DF(bool df) {
#ifdef _WIN32
            auto errv4 = set_dontfragment_v6(df);
            auto errv6 = set_dontfragment(df);
#else
            auto errv4 = set_mtu_discover(df);
            auto errv6 = set_mtu_discover_v6(df);
#endif
            if (errv4 || errv6) {
                if (errv4 && errv6) {
                    return SetDFError{std::move(errv4), std::move(errv6)};
                }
                return errv4 ? errv4 : errv6;
            }
            return error::none;
        }
    }  // namespace fnet
}  // namespace utils
