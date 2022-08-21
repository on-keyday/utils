/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstdint>
#include <utility>
#include "dll/dllh.h"

namespace utils {
    namespace dnet {
        struct SockAddr {
            union {
                struct {
                    const void* addr;
                    int addrlen;
                };
                struct {
                    const char* hostname;
                    int namelen;
                };
            };
            int af;
            int type;
            int proto;
            int flag;
        };

        struct class_export AddrInfo {
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
            constexpr const void* current() const {
                return select;
            }
            const void* sockaddr(SockAddr& info) const;
            constexpr AddrInfo()
                : AddrInfo(nullptr) {}
            ~AddrInfo();
        };

        struct class_export WaitAddrInfo {
           private:
            void* opt;
            int err;
            constexpr WaitAddrInfo(void* o, int e)
                : opt(o), err(e) {}
            friend dll_export(WaitAddrInfo) resolve_address(const SockAddr& addr, const char* port);

           public:
            constexpr WaitAddrInfo(WaitAddrInfo&& info)
                : opt(std::exchange(info.opt, nullptr)) {}
            constexpr WaitAddrInfo& operator=(WaitAddrInfo&& info) {
                if (this == &info) {
                    return *this;
                }
                opt = std::exchange(info.opt, nullptr);
                return *this;
            }
            bool failed(int* err = nullptr) const;
            AddrInfo wait() {
                AddrInfo info;
                wait(info, ~0);
                return info;
            }
            bool wait(AddrInfo& info, std::uint32_t time);
            constexpr WaitAddrInfo()
                : opt(nullptr), err() {}
            ~WaitAddrInfo();
        };

        [[nodiscard]] dll_export(WaitAddrInfo) resolve_address(const SockAddr& addr, const char* port);
    }  // namespace dnet
}  // namespace utils
