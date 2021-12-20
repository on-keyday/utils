/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// dns - dns query
#pragma once

#include "../../wrap/lite/smart_ptr.h"

namespace utils {
    namespace net {
        namespace internal {
            //NOTE: using pimple pattern to remove platform dependency from header
            struct DnsResultImpl;
            struct AddressImpl;
        }  // namespace internal

        struct Address {
            friend struct DnsResult;
            Address();
            ~Address();

            void* get_rawaddr();

           private:
            internal::AddressImpl* impl = nullptr;
        };

        struct DnsResult {
            friend DnsResult query_dns(const char* host, const char* port, time_t timeout_sec,
                                       int address_family, int socket_type, int protocol, int flags);
            constexpr DnsResult() {}

            DnsResult(const DnsResult&) = delete;

            DnsResult(DnsResult&&) noexcept;

            DnsResult& operator=(const DnsResult&) = delete;

            DnsResult& operator=(DnsResult&&) noexcept;

            ~DnsResult();

            wrap::shared_ptr<Address> get_address();

            bool failed();

            void cancel();

           private:
            internal::DnsResultImpl* impl = nullptr;
        };

        DnsResult query_dns(const char* host, const char* port, time_t timeout_sec,
                            int address_family = 0, int socket_type = 0, int protocol = 0, int flags = 0);
        wrap::shared_ptr<Address> from_sockaddr_st(void* st);
    }  // namespace net
}  // namespace utils
