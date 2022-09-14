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
#include "../core/strlen.h"

namespace utils {
    namespace dnet {

        // SockAddr wraps native addrinfo representation
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
            int af = 0;
            int type = 0;
            int proto = 0;
            int flag = 0;
        };
#ifdef _WIN32
        constexpr auto INET6_ADDRSTRLEN_ = 65;
#else
        constexpr auto INET6_ADDRSTRLEN_ = 46;
#endif

        struct IPText {
            char text[INET6_ADDRSTRLEN_ + 1]{0};
            constexpr const char* c_str() const {
                return text;
            }
            constexpr size_t size() const {
                return utils::strlen(text);
            }

            constexpr char operator[](size_t i) const {
                return text[i];
            }
        };

        // AddrInfo is wrapper class of getaddrinfo functions
        // these operations are non blocking
        struct dnet_class_export AddrInfo {
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
            constexpr const void* current() const {
                return select;
            }
            constexpr void reset_iterator() {
                select = nullptr;
            }
            const void* sockaddr(SockAddr& info) const;
            bool string(void* text, size_t len, int* port = nullptr, int* err = nullptr) const;

            IPText string(int* port = nullptr, int* err = nullptr) const {
                if (err) {
                    *err = 0;
                }
                IPText text;
                string(text.text, sizeof(text.text), port, err);
                return text;
            }

            constexpr AddrInfo()
                : AddrInfo(nullptr) {}
            ~AddrInfo();
        };

        dnet_dll_export(bool) string_from_sockaddr(const void* addr, size_t addrlen, void* text, size_t len, int* port, int* err);

        inline IPText string_from_sockaddr(const void* addr, size_t addrlen, int* port = nullptr, int* err = nullptr) {
            if (err) {
                *err = 0;
            }
            IPText text;
            string_from_sockaddr(addr, addrlen, text.text, sizeof(text.text), port, err);
            return text;
        }

        // WaitAddrInfo is waiter calss of dns resolevement
        struct dnet_class_export WaitAddrInfo {
           private:
            void* opt;
            int err;
            constexpr WaitAddrInfo(void* o, int e)
                : opt(o), err(e) {}
            friend dnet_dll_export(WaitAddrInfo) resolve_address(const SockAddr& addr, const char* port);
            friend dnet_dll_export(WaitAddrInfo) get_self_host_address(const SockAddr& addr, const char* port);

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
            bool cancel();
            constexpr WaitAddrInfo()
                : opt(nullptr), err() {}
            ~WaitAddrInfo();
        };

        [[nodiscard]] dnet_dll_export(WaitAddrInfo) resolve_address(const SockAddr& addr, const char* port);

        // this invokes resolve_address with addr.flag|=AI_PASSIVE and addr.hostname=nullptr, addr.namelen=0
        [[nodiscard]] dnet_dll_export(WaitAddrInfo) get_self_server_address(const SockAddr& addr, const char* port);

        // this invokes resolve_address with addr.hostname=gethostname()
        [[nodiscard]] dnet_dll_export(WaitAddrInfo) get_self_host_address(const SockAddr& addr, const char* port = nullptr);
    }  // namespace dnet
}  // namespace utils