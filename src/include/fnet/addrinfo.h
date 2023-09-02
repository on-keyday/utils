/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstdint>
#include <utility>
#include "dll/dllh.h"
#include "../core/strlen.h"
#include "address.h"
#include "error.h"
#include "ip/protocol.h"

namespace utils {
    namespace fnet {

        // SockAddr wraps native addrinfo representation
        struct SockAddr {
            NetAddrPort addr;
            SockAttr attr;
        };

        // AddrInfo is wrapper class of getaddrinfo functions
        // these operations are non blocking
        struct fnet_class_export AddrInfo {
           private:
            void* root;
            void* select;
            friend struct WaitAddrInfo;
            constexpr AddrInfo(void* root)
                : root(root), select(nullptr){};

           public:
            constexpr AddrInfo(AddrInfo&& in)
                : root(std::exchange(in.root, nullptr)),
                  select(std::exchange(in.select, nullptr)) {}
            constexpr AddrInfo& operator=(AddrInfo&& in) {
                if (this == &in) {
                    return *this;
                }
                root = std::exchange(in.root, nullptr);
                select = std::exchange(in.select, nullptr);
                return *this;
            }
            void* next();
            constexpr const void* exists() const {
                return root;
            }

            constexpr void reset_iterator() {
                select = nullptr;
            }

            SockAddr sockaddr() const;

            constexpr AddrInfo()
                : AddrInfo(nullptr) {}
            ~AddrInfo();
        };

        // WaitAddrInfo is waiter class of dns resolevement
        struct fnet_class_export WaitAddrInfo {
           private:
            void* opt;
            // error::Error err;
            constexpr WaitAddrInfo(void* o)
                : opt(o) {}
            friend fnet_dll_export(expected<WaitAddrInfo>) resolve_address(view::rvec hostname, view::rvec port, SockAttr attr);
            friend fnet_dll_export(expected<WaitAddrInfo>) get_self_host_address(view::rvec port, SockAttr attr);

           public:
            constexpr WaitAddrInfo(WaitAddrInfo&& info)
                : opt(std::exchange(info.opt, nullptr)) {}
            constexpr WaitAddrInfo& operator=(WaitAddrInfo&& info) {
                if (this == &info) {
                    return *this;
                }
                this->~WaitAddrInfo();
                opt = std::exchange(info.opt, nullptr);
                return *this;
            }

            expected<AddrInfo> wait(std::uint32_t time = ~0) {
                AddrInfo info;
                auto err = wait(info, time);
                if (err) {
                    return unexpect(err);
                }
                return std::move(info);
            }

            error::Error wait(AddrInfo& info, std::uint32_t time);
            error::Error cancel();
            constexpr WaitAddrInfo()
                : opt(nullptr) {}
            ~WaitAddrInfo();
        };

        // you MUST set attr to find address
        [[nodiscard]] fnet_dll_export(expected<WaitAddrInfo>) resolve_address(view::rvec hostname, view::rvec port, SockAttr attr);

        // this invokes resolve_address with attr.flag|=AI_PASSIVE and hostname = {}
        [[nodiscard]] fnet_dll_export(expected<WaitAddrInfo>) get_self_server_address(view::rvec port, SockAttr attr);

        // this invokes resolve_address with hostname=gethostname()
        [[nodiscard]] fnet_dll_export(expected<WaitAddrInfo>) get_self_host_address(view::rvec port, SockAttr attr);

        fnet_dll_export(SockAttr) sockattr_tcp(ip::Version ipver = ip::Version::unspec);
        fnet_dll_export(SockAttr) sockattr_udp(ip::Version ipver = ip::Version::unspec);
        fnet_dll_export(SockAttr) sockattr_raw(ip::Version ipver = ip::Version::ipv4, ip::Protocol protocol = ip::Protocol::udp);  //

    }  // namespace fnet
}  // namespace utils
