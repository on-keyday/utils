/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// dns - dns query
#pragma once
#include <platform/windows/dllexport_header.h>
#include <wrap/light/smart_ptr.h>
#include "../generate/iocloser.h"
// dnet library
#include <dnet/addrinfo.h>

namespace utils {
    namespace net {
        struct Address;
        namespace internal {
            // NOTE: using pimpl pattern to remove platform dependency from header
            struct DnsResultImpl;
            struct AddressImpl;
            wrap::shared_ptr<Address> from_sockaddr_st(void* st, int len);

        }  // namespace internal

        struct DLL Address {
            friend wrap::shared_ptr<Address> internal::from_sockaddr_st(void* st, int len);
            friend struct DnsResult;
            Address();
            ~Address();

            void* get_rawaddr();

            bool stringify(IBuffer buf, size_t index = 0);

            void set_limit(size_t sock);

            constexpr dnet::AddrInfo& get_dnaddrinfo() {
                return addr;
            }

           private:
            dnet::AddrInfo addr;
            void* saddr = nullptr;
        };

        enum class IpAddrLimit {
            none,
            v4,
            v6,
        };

        struct DLL DnsResult {
            friend DLL DnsResult STDCALL query_dns(const char* host, const char* port, time_t timeout_sec,
                                                   int address_family, int socket_type, int protocol, int flags);
            constexpr DnsResult() {}

            wrap::shared_ptr<Address> get_address();

            bool failed();

            void cancel();

           private:
            dnet::WaitAddrInfo wait;
        };

        DLL DnsResult STDCALL query_dns(const char* host, const char* port, time_t timeout_sec = 60,
                                        int address_family = 0, int socket_type = 0, int protocol = 0, int flags = 0);

    }  // namespace net
}  // namespace utils
