/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../../include/net/core/platform.h"
#include "../../../include/net/dns/dns.h"

#include "../../../include/wrap/lite/string.h"
#include "../../../include/utf/convert.h"

namespace utils {
    namespace net {
        namespace internal {
#ifdef _WIN32
            using addrinfo = ::addrinfoexW;
#else
            using addrinfo = ::addrinfo;
#endif
            struct DnsResultImpl {
                addrinfo* result;
#ifdef _WIN32
                ::OVERLAPPED overlapped;
                ::HANDLE cancel = nullptr;
#endif
            };
        }  // namespace internal

        DnsResult::~DnsResult() {
            delete impl;
        }

        DnsResult query_dns(const char* host, const char* port, time_t timeout_sec,
                            int address_family, int socket_type, int protocol, int flags) {
            internal::addrinfo hint = {0};
            hint.ai_family = address_family;
            hint.ai_socktype = socket_type;
            hint.ai_protocol = protocol;
            hint.ai_flags = flags;
            DnsResult result;
            result.impl = new internal::DnsResultImpl();
            auto impl = result.impl;
#ifdef _WIN32
            impl->overlapped.hEvent = ::CreateEventA(NULL, true, false, nullptr);
            if (impl->overlapped.hEvent == nullptr) {
                return DnsResult();
            }
            auto host_ = utf::convert<wrap::path_string>(host);
            auto port_ = utf::convert<wrap::path_string>(port);
            ::timeval timeout{
                .tv_sec = timeout_sec,
                .tv_usec = 0,
            };
            ::GetAddrInfoExW(host_.c_str(), port_.c_str(), NS_DNS, nullptr,
                             &hint, &impl->result, &timeout, &impl->overlapped, nullptr, &impl->cancel);
#endif
        }
    }  // namespace net
}  // namespace utils
