/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <platform/windows/dllexport_source.h>
#include <deprecated/net/core/platform.h>
#include <deprecated/net/core/init_net.h>
#include <deprecated/net/dns/dns.h>

#include <wrap/light/string.h>
#include <utf/convert.h>
#include <helper/strutil.h>

#include <cassert>

namespace utils {
    namespace net {
        namespace internal {

            wrap::shared_ptr<Address> from_sockaddr_st(void* st, int len) {
                auto cst = new (std::nothrow)::sockaddr_storage(*static_cast<sockaddr_storage*>(st));
                if (!cst) {
                    return nullptr;
                }
                auto addr = wrap::make_shared<Address>();
                addr->saddr = cst;
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
        Address::Address() {}

        Address::~Address() {
            if (saddr) {
                auto st = static_cast<sockaddr_storage*>(saddr);
                delete st;
            }
        }

        void Address::set_limit(size_t sock) {
            // if (!impl) return;
            /*switch (impl->limit) {
                case IpAddrLimit::v4: {
                }
                case IpAddrLimit::v6: {
                    u_long flag = 1;
                    ::setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&flag, sizeof(flag));
                }
                default:
                    return;
            }*/
            // TODO(on-keyday): FIX place or function
        }

        bool Address::stringify(IBuffer buf, size_t index) {
            if (!addr.exists()) {
                if (saddr) {
                    auto text = dnet::string_from_sockaddr(saddr, sizeof(sockaddr_storage));
                    helper::append(buf, text);
                    return true;
                }
            }
            const auto curp = addr.current();
            addr.reset_iterator();
            auto reset_curp = [&] {
                addr.reset_iterator();
                while (addr.current() != curp) {
                    addr.next();
                }
            };
            size_t count = 0;
            while (addr.next() && count < index) {
                count++;
            }
            if (!addr.current()) {
                reset_curp();
                return false;
            }
            auto text = addr.string();
            helper::append(buf, text);
            return true;
        }

        void* Address::get_rawaddr() {
            return const_cast<void*>(addr.exists());
        }

        bool DnsResult::failed() {
            return wait.failed();
        }

        void DnsResult::cancel() {
            wait.cancel();
        }

        wrap::shared_ptr<Address> DnsResult::get_address() {
            dnet::AddrInfo info;
            if (wait.wait(info, 1)) {
                auto addr = wrap::make_shared<Address>();
                addr->addr = std::move(info);
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
            dnet::SockAddr addr{};
            addr.hostname = host;
            addr.namelen = strlen(host);
            addr.af = address_family;
            addr.type = socket_type;
            addr.proto = protocol;
            addr.flag = flags;
            result.wait = dnet::resolve_address(addr, port);
            return result;
        }

    }  // namespace net
}  // namespace utils
