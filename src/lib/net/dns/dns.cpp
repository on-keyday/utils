/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../../include/platform/windows/dllexport_source.h"
#include "../../../include/net/core/platform.h"
#include "../../../include/net/core/init_net.h"
#include "../../../include/net/dns/dns.h"

#include "../../../include/wrap/lite/string.h"
#include "../../../include/utf/convert.h"
#include "../../../include/helper/strutil.h"

#include <cassert>

namespace utils {
    namespace net {
        namespace internal {

#ifdef _WIN32
            void freeaddrinfo(addrinfo* info) {
                FreeAddrInfoExW(info);
            }
#else
            void freeaddrinfo(addrinfo* info) {
                ::freeaddrinfo(info);
            }
#endif
            struct AddressImpl {
                addrinfo* result = nullptr;
                bool from_sockaddr = false;
                IpAddrLimit limit = IpAddrLimit::none;

                ~AddressImpl() {
                    if (from_sockaddr) {
                        if (!result) {
                            return;
                        }
                        delete reinterpret_cast<::sockaddr_storage*>(result->ai_addr);
                        delete result;
                    }
                    else {
                        internal::freeaddrinfo(result);
                    }
                }
            };

            struct DnsResultImpl {
                addrinfo* result;
                bool failed;
#ifdef _WIN32
                ::OVERLAPPED overlapped;
                ::HANDLE cancel = nullptr;
                bool running = true;

#endif

                bool complete_query() {
#ifdef _WIN32
                    auto done = ::WaitForSingleObject(overlapped.hEvent, 2);
                    if (done == WAIT_OBJECT_0) {
                        auto res = ::GetAddrInfoExOverlappedResult(&overlapped);
                        if (res != WSAEINPROGRESS) {
                            running = false;
                            ::ResetEvent(overlapped.hEvent);
                            if (res != NO_ERROR) {
                                failed = true;
                                return false;
                            }
                            return true;
                        }
                    }
#endif
                    return false;
                }

                void cancel_impl() {
#ifdef _WIN32
                    if (running) {
                        ::GetAddrInfoExCancel(&cancel);
                        running = false;
                    }
#endif
                }

                ~DnsResultImpl() {
                    cancel_impl();
#ifdef _WIN32
                    if (overlapped.hEvent != nullptr) {
                        ::ResetEvent(overlapped.hEvent);
                        ::CloseHandle(overlapped.hEvent);
                    }
                    if (result) {
                        freeaddrinfo(result);
                    }
#endif
                }
            };

            wrap::shared_ptr<Address> from_sockaddr_st(void* st, int len) {
                auto storage = reinterpret_cast<::sockaddr_storage*>(st);
                auto info = new addrinfo();
                info->ai_addrlen = len;
                info->ai_addr = reinterpret_cast<::sockaddr*>(new ::sockaddr_storage(std::move(*storage)));
                auto addr = wrap::make_shared<Address>();
                addr->impl->from_sockaddr = true;
                addr->impl->result = info;
                return addr;
            }
        }  // namespace internal

#ifdef _WIN32
        static void format_error(wrap::string& errmsg, int err = ::WSAGetLastError()) {
            wchar_t* ptr = nullptr;
            auto result = ::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, err, 0, (LPWSTR)&ptr, 0, nullptr);
            errmsg.clear();
            errmsg = std::to_string(err) + ":";
            errmsg += utf::convert<wrap::string>(ptr);
            ::LocalFree(ptr);
        }
#else
        static void format_error(wrap::string& errmsg, int err = 0) {}
#endif
        Address::Address() {
            impl = new internal::AddressImpl();
        }

        Address::~Address() {
            delete impl;
        }

        void Address::set_limit(size_t sock) {
            if (!impl) return;
            switch (impl->limit) {
                case IpAddrLimit::v4: {
                }
                case IpAddrLimit::v6: {
                    u_long flag = 1;
                    ::setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&flag, sizeof(flag));
                }
                default:
                    return;
            }
        }

        bool Address::stringify(IBuffer buf, size_t index) {
            if (!impl) {
                return false;
            }
            auto sel = impl->result;
            size_t count = 0;
            while (sel && count < index) {
                sel = sel->ai_next;
            }
            if (!sel) {
                return false;
            }
            char tmp[75] = {0};
            if (sel->ai_family == AF_INET) {
                sockaddr_in* addr4 = (sockaddr_in*)sel->ai_addr;
                inet_ntop(sel->ai_family, &addr4->sin_addr, tmp, 75);
            }
            else if (sel->ai_family == AF_INET6) {
                sockaddr_in6* addr6 = (sockaddr_in6*)sel->ai_addr;
                inet_ntop(sel->ai_family, &addr6->sin6_addr, tmp, 75);
            }
            size_t start_idx = 0;
            if (helper::contains(tmp, ".") && helper::starts_with(tmp, "::ffff:")) {
                start_idx = 7;
            }
            helper::append(buf, tmp + start_idx);
            return true;
        }

        void* Address::get_rawaddr() {
            if (!impl) {
                return nullptr;
            }
            return reinterpret_cast<void*>(impl->result);
        }

        DnsResult::~DnsResult() {
            delete impl;
        }

        DnsResult::DnsResult(DnsResult&& in) noexcept {
            impl = in.impl;
            in.impl = nullptr;
        }

        DnsResult& DnsResult::operator=(DnsResult&& in) noexcept {
            delete impl;
            impl = in.impl;
            in.impl = nullptr;
            return *this;
        }

        bool DnsResult::failed() {
            if (!impl) {
                return true;
            }
            if (impl->failed) {
                return true;
            }
            return false;
        }

        void DnsResult::cancel() {
            if (impl) {
                impl->cancel_impl();
            }
        }

        wrap::shared_ptr<Address> DnsResult::get_address() {
            if (!impl) {
                return nullptr;
            }
            if (impl->complete_query()) {
                auto addr = wrap::make_shared<Address>();
                addr->impl->result = impl->result;
                impl->result = nullptr;
                return addr;
            }
            return nullptr;
        }

        DnsResult query_dns(const char* host, const char* port, time_t timeout_sec,
                            int address_family, int socket_type, int protocol, int flags) {
            network();
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
            ::timeval timeout{0};
            timeout.tv_sec = timeout_sec;
            auto v = ::GetAddrInfoExW(host_.c_str(), port_.c_str(), NS_DNS, nullptr,
                                      &hint, &impl->result, &timeout, &impl->overlapped, nullptr, &impl->cancel);
            if (v != NO_ERROR && v != WSA_IO_PENDING) {
                return DnsResult();
            }
#else
            assert(false && "unimplemented");
#endif
            return result;
        }

    }  // namespace net
}  // namespace utils
