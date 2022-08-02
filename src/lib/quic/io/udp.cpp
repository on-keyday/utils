/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// udp - io udp wrapper
#include <quic/common/dll_cpp.h>
#include <quic/io/io.h>
#include <quic/io/udp.h>
#include <quic/mem/alloc.h>
#include <quic/common/variable_int.h>
#include <quic/internal/sock_internal.h>
#include <quic/internal/external_func_internal.h>

// for initialization
#include <quic/mem/once.h>

namespace utils {
    namespace quic {
        namespace io {
            namespace ext = external;
            bool sockaddr_to_target(::sockaddr_storage* storage, Target* t) {
                if (storage->ss_family == AF_INET) {
                    auto addr = reinterpret_cast< ::sockaddr_in*>(storage);
                    t->key = udp::UDP_IPv4;
                    varint::Swap<uint> swp{addr->sin_addr.s_addr};
                    t->ip.address[0] = swp.swp[0];
                    t->ip.address[1] = swp.swp[1];
                    t->ip.address[2] = swp.swp[2];
                    t->ip.address[3] = swp.swp[3];
                    t->ip.port = varint::swap_if(addr->sin_port);
                    return true;
                }
                if (storage->ss_family == AF_INET6) {
                    auto addr = reinterpret_cast< ::sockaddr_in6*>(storage);
                    t->key = udp::UDP_IPv6;
                    bytes::copy(t->ip.address, addr->sin6_addr.s6_addr, 16);
                    t->ip.port = varint::swap_if(addr->sin6_port);
                    return true;
                }
                return false;
            }

            namespace sys {
#ifdef _WIN32
                struct Info {
                    ::WSAData wsadata;
                    int last_code;
                };

                int initialize_network_windows(Info* info) {
                    ext::load_SockDll(
                        [](ext::SockDll* dll, void* user) {
                            auto info = static_cast<Info*>(user);
                            if (!dll) {
                                info->last_code = -1;
                                return;
                            }
                            info->last_code = dll->WSAStartup_(MAKEWORD(2, 2), &info->wsadata);
                        },
                        info);
                    return info->last_code;
                }

                void uninitialize_network_windows(Info* info) {
                    ext::unload_SockDll(
                        [](ext::SockDll* dll, void* user) {
                            dll->WSACleanup_();
                            auto info = static_cast<Info*>(user);
                            info->last_code = -1;
                        },
                        info);
                }

                struct System {
                    Info info;

                    ~System() {
                        uninitialize_network_windows(&info);
                    }
                    Info* get_info() {
                        return &info;
                    }

                    bool ready() {
                        initialize_network_windows(&info);
                        return info.last_code == 0;
                    }

                    int code() {
                        return info.last_code;
                    }
                };
#else
                struct Info;
                struct System {
                    Info* get_info() {
                        return nullptr;
                    }

                    bool ready() {
                        return true;
                    }

                    int code() {
                        return 0;
                    }
                };
#endif
            }  // namespace sys

            namespace udp {

                struct Conn {
                };

                io::Result read_from(Conn* c, Target* t, byte* d, tsize l) {
                    if (!ext::sockdll.loaded()) {
                        return {io::Status::fatal, invalid, true};
                    }
                    if (!t) {
                        return {io::Status::unsupported, invalid, true};
                    }
                    if (t->target == invalid || t->target == global) {
                        return {io::Status::invalid_argument, invalid, true};
                    }
                    ::sockaddr_storage storage;
                    int len = sizeof(storage);
                    auto err = ext::sockdll.recvfrom_(t->target, topchar(d), l, 0, repaddr(&storage), &len);
                    if (err == -1) {
                        return with_code(GET_ERROR());
                    }
                    if (!sockaddr_to_target(&storage, t)) {
                        return {io::Status::unsupported, storage.ss_family};
                    }
                    return {io::Status::ok, tsize(err)};
                }

                io::Result write_to(Conn* c, const Target* t, const byte* d, tsize l) {
                    if (!external::sockdll.loaded()) {
                        return {io::Status::fatal, invalid, true};
                    }
                    if (!t || t->key != UDP_IPv4 && t->key != UDP_IPv6) {
                        return {io::Status::unsupported, invalid, true};
                    }
                    if (t->target == invalid || t->target == global) {
                        return {io::Status::invalid_argument, invalid, true};
                    }
                    return target_to_sockaddr(t, [&](::sockaddr* addr, tsize alen) -> io::Result {
                        auto err = ext::sockdll.sendto_(t->target, topchar(d), l, 0, addr, alen);
                        if (err == -1) {
                            return with_code(GET_ERROR());
                        }
                        return {io::Status::ok, tsize(err)};
                    });
                }

                sys::System sys;

                io::Result new_target(Conn* a, const Target* hint, Mode mode) {
                    if (!sys.ready()) {
                        return {io::Status::fatal, ErrorCode(sys.code())};
                    }
                    if (!hint || (hint->key != UDP_IPv4 && hint->key != UDP_IPv6)) {
                        return {io::Status::unsupported, invalid, true};
                    }
                    int af = AF_INET;
                    if (hint->key == UDP_IPv6) {
                        af = AF_INET6;
                    }
#ifdef _WIN32
                    auto sock = ext::sockdll.WSASocketW_(af, SOCK_DGRAM, 0, nullptr, 0, WSA_FLAG_OVERLAPPED);
#else
                    auto sock = ::socket(af, SOCK_DGRAM, 0);
#endif
                    if (sock == -1) {
                        return with_code(GET_ERROR());
                    }
                    u_long val = 1;
                    auto err = ext::sockdll.ioctlsocket_(sock, FIONBIO, &val);
                    if (err == -1) {
                        return with_code(GET_ERROR());
                    }
                    // for quic
                    // DO NOT fragment packet
                    val = IP_PMTUDISC_DO;
                    err = ext::sockdll.setsockopt_(sock, IPPROTO_IP, IP_MTU_DISCOVER, reinterpret_cast<const char*>(&val), sizeof(val));
                    if (err == -1) {
                        return with_code(GET_ERROR());
                    }
                    if (mode == server) {
                        if (!target_to_sockaddr(hint, [&](::sockaddr* s, tsize len) {
                                auto err = ext::sockdll.bind_(sock, s, len);
                                if (err == -1) {
                                    return false;
                                }
                                return true;
                            })) {
                            ext::sockdll.closesocket_(sock);
                            return with_code(GET_ERROR());
                        }
                    }
                    return {Status::ok, TargetID{sock}};
                }

                io::Result discard_target(Conn* c, TargetID id) {
                    if (!ext::sockdll.loaded()) {
                        return {io::Status::fatal, invalid, true};
                    }
                    auto err = ext::sockdll.closesocket_(id);
                    if (err == -1) {
                        return {Status::failed, tsize(GET_ERROR())};
                    }
                    return {Status::ok};
                }

                io::Result option(Conn* c, Option* o) {
                    if (!ext::sockdll.loaded()) {
                        return {io::Status::fatal, invalid, true};
                    }
                    if (o->key == MTU) {
                        int mtu = 0;
                        int len = sizeof(mtu);
                        auto err = ext::sockdll.getsockopt_(o->target, IPPROTO_IP, IP_MTU, reinterpret_cast<char*>(&mtu), &len);
                        if (err == -1) {
                            return {io::Status::failed, tsize(GET_ERROR())};
                        }
                        o->rn = mtu;
                        return {io::Status::ok};
                    }
                    else if (o->key == Connect) {
                        auto target = static_cast<Target*>(o->ptr);
                        if (!target ||
                            (target->key != UDP_IPv4 &&
                             target->key != UDP_IPv6)) {
                            return {io::Status::unsupported, invalid, true};
                        }
                        return target_to_sockaddr(target, [&](::sockaddr* addr, tsize len) -> io::Result {
                            auto err = ext::sockdll.connect_(o->target, addr, len);
                            if (err == -1) {
                                return with_code(GET_ERROR());
                            }
                            return {io::Status::ok};
                        });
                    }
                    return {io::Status::unsupported, invalid, true};
                }

                dll(io::IO) Protocol(allocate::Alloc* a) {
                    io::IOComponent<Conn> component{
                        .option = option,
                        .write_to = write_to,
                        .read_from = read_from,
                        .new_target = new_target,
                        .discard_target = discard_target,
                    };
                    return component.to_IO();
                }
            }  // namespace udp
        }      // namespace io
    }          // namespace quic
}  // namespace utils
