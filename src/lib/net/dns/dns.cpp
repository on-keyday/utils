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

                ~AddressImpl() {
                    freeaddrinfo(result);
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
#endif
                    return false;
                }

                void cancel_impl() {
                    if (running) {
                        ::GetAddrInfoExCancel(&cancel);
                        running = false;
                    }
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
