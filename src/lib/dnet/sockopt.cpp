/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/dllcpp.h>
#include <dnet/socket.h>
#include <dnet/plthead.h>
#include <dnet/dll/asyncbase.h>
#include <dnet/dll/sockdll.h>

namespace utils {
    namespace dnet {
        error::Error Socket::get_localaddr(raw_address* addr, int* addrlen) {
            if (!addrlen) {
                return error::Error(invalid_addr, error::ErrorCategory::validationerr);
            }
            socklen_t len = 0;
            auto res = socdl.getsockname_(sock, reinterpret_cast<sockaddr*>(addr), &len);
            if (res != 0) {
                return Errno();
            }
            *addrlen = len;
            return error::none;
        }

        std::pair<NetAddrPort, error::Error> Socket::get_localaddr() {
            sockaddr_storage st{};
            socklen_t len = sizeof(st);
            auto addr = reinterpret_cast<sockaddr*>(&st);
            auto res = socdl.getsockname_(sock, addr, &len);
            if (res != 0) {
                return {{}, Errno()};
            }
            return {sockaddr_to_NetAddrPort(addr, len), error::none};
        }

        error::Error Socket::get_remoteaddr(raw_address* addr, int* addrlen) {
            if (!addrlen) {
                return error::Error(invalid_addr, error::ErrorCategory::validationerr);
            }
            socklen_t len = 0;
            auto res = socdl.getpeername_(sock, reinterpret_cast<sockaddr*>(addr), &len);
            if (res != 0) {
                return Errno();
            }
            *addrlen = len;
            return error::none;
        }
        std::pair<NetAddrPort, error::Error> Socket::get_remoteaddr() {
            sockaddr_storage st{};
            socklen_t len = sizeof(st);
            auto addr = reinterpret_cast<sockaddr*>(&st);
            auto res = socdl.getpeername_(sock, addr, &len);
            if (res != 0) {
                return {{}, Errno()};
            }
            return {sockaddr_to_NetAddrPort(addr, len), error::none};
        }

        error::Error Socket::get_option(int layer, int opt, void* buf, size_t size) {
            socklen_t len = int(size);
            auto res = socdl.getsockopt_(sock, layer, opt, static_cast<char*>(buf), &len);
            if (res != 0) {
                return Errno();
            }
            return error::none;
        }

        error::Error Socket::set_option(int layer, int opt, const void* buf, size_t size) {
            auto res = socdl.setsockopt_(sock, layer, opt, static_cast<const char*>(buf), int(size));
            if (res != 0) {
                return Errno();
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
            return error::Error(not_supported, error::ErrorCategory::dneterr);
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
                return error::Error(invalid_argument, error::ErrorCategory::validationerr);
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
                return error::Error(invalid_argument, error::ErrorCategory::validationerr);
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
            return error::Error(not_supported, error::ErrorCategory::dneterr);
#endif
        }

        error::Error Socket::set_dontfragment_v6(bool df) {
            return set_option(IPPROTO_IP, IPV6_DONTFRAG, std::uint32_t(df ? 1 : 0));
        }

        error::Error Socket::set_connreset(bool enable) {
#ifdef _WIN32
            std::int32_t flag = enable ? 1 : 0;
            DWORD ret = 0;
            auto err = socdl.WSAIoctl_(sock, SIO_UDP_CONNRESET, &flag, sizeof(flag), nullptr, 0, &ret, nullptr, nullptr);
            if (err == SOCKET_ERROR) {
                return Errno();
            }
            return error::none;
#else
            return error::Error(not_supported, error::ErrorCategory::dneterr);
#endif
        }

        std::tuple<int, int, int, error::Error> Socket::get_af_type_protocol() {
#ifdef _WIN32
            WSAPROTOCOL_INFOW info{};
            auto err = get_option(SOL_SOCKET, SO_PROTOCOL_INFOW, info);
            if (err) {
                return {-1, -1, -1, err};
            }
            return {info.iAddressFamily, info.iSocketType, info.iProtocol, error::none};
#else
            int af = -1, type = -1, proto = -1;
            auto err = get_option(SOL_SOCKET, SO_DOMAIN, af);
            if (err) {
                return {-1, -1, -1, err};
            }
            err = get_option(SOL_SOCKET, SO_TYPE, type);
            if (err) {
                return {af, -1, -1, err};
            }
            err = get_option(SOL_SOCKET, SO_PROTOCOL, proto);
            if (err) {
                return {af, type, -1, err};
            }
            return {af, type, proto, error::none};
#endif
        }
    }  // namespace dnet
}  // namespace utils
