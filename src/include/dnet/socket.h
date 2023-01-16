/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// native_socket - native socket interface
#pragma once
#include <cstdint>
#include <cstddef>
#include <utility>
#include <memory>
#include "dll/dllh.h"
#include "errcode.h"
#include "heap.h"
#include "../helper/defer.h"
#include "dll/glheap.h"
#include "address.h"
#include "error.h"

namespace utils {
    namespace dnet {
        // raw_address is maker of sockaddr
        // cast sockaddr to raw_address
        struct raw_address;
        struct AsyncSuite;

        // wait_event waits io completion until time passed
        // if event is processed function returns number of event
        // otherwise returns 0
        dnet_dll_export(int) wait_event(std::uint32_t time);

        enum MTUConfig {
            mtu_default,  // same as IP_PMTUDISC_WANT
            mtu_enable,   // same as IP_PMTUDISC_DO, DF flag set on ip layer
            mtu_disable,  // same as IP_PMTUDISC_DONT
            mtu_ignore,   // same as IP_PMTUDISC_PROBE
        };

        dnet_dll_export(bool) isSysBlock(const error::Error&);
        dnet_dll_export(bool) isMsgSize(const error::Error& err);

        // Socket is wrappper class of native socket
        struct dnet_class_export Socket {
           private:
            std::uintptr_t sock = ~0;
            void* async_ctx = nullptr;

            constexpr Socket(std::uintptr_t s)
                : sock(s) {}
            error::Error get_option(int layer, int opt, void* buf, size_t size);
            error::Error set_option(int layer, int opt, const void* buf, size_t size);
            friend Socket make_socket(std::uintptr_t uptr);

            auto wrap_tuple(auto&&... args) {
                return helper::wrapfn_ptr(
                    std::make_tuple(std::forward<decltype(args)>(args)...), [](size_t size, size_t align) { return alloc_normal(size, align, DNET_DEBUG_MEMORY_LOCINFO(true, size, align)); },
                    [](void* p) {
                        free_normal(p, DNET_DEBUG_MEMORY_LOCINFO(false, 0, 0));
                    });
            }

           public:
            constexpr Socket() = default;
            ~Socket();
            constexpr Socket(Socket&& sock)
                : sock(std::exchange(sock.sock, ~std::uintptr_t(0))),
                  async_ctx(std::exchange(sock.async_ctx, nullptr)) {}
            Socket& operator=(Socket&& sock) {
                if (this == &sock) {
                    return *this;
                }
                this->~Socket();
                this->sock = std::exchange(sock.sock, ~0);
                async_ctx = std::exchange(sock.async_ctx, nullptr);
                return *this;
            }

            error::Error connect(const raw_address* addr /* = sockaddr*/, size_t len);
            error::Error connect(const NetAddrPort& addr);

            error::Error get_localaddr(raw_address* addr, int* addrlen);
            std::pair<NetAddrPort, error::Error> get_localaddr();
            error::Error get_remoteaddr(raw_address* addr, int* addrlen);
            std::pair<NetAddrPort, error::Error> get_remoteaddr();

            // returns (remain,err)
            std::pair<view::rvec, error::Error> write(view::rvec data, int flag = 0);

            // returns (read,err)
            std::pair<view::wvec, error::Error> read(view::wvec data, int flag = 0, bool is_stream = true);
            // std::pair<size_t, error::Error> writeto(const raw_address* addr, int addrlen, const void* data, size_t len, int flag = 0);

            // returns (remain,err)
            std::pair<view::rvec, error::Error> writeto(const NetAddrPort& addr, view::rvec data, int flag = 0);
            // std::pair<size_t, error::Error> readfrom(raw_address* addr, int* addrlen, void* data, size_t len, int flag = 0, bool is_stream = false);

            std::tuple<view::wvec, NetAddrPort, error::Error> readfrom(view::wvec data, int flag = 0, bool is_stream = false);

            // [[nodiscard]] error::Error accept(Socket& to, raw_address* addr, int* addrlen);
            [[nodiscard]] std::tuple<Socket, NetAddrPort, error::Error> accept();
            error::Error bind(const raw_address* addr /* = sockaddr*/, size_t len);

            error::Error bind(const NetAddrPort& addr);

            error::Error listen(int backlog = 10);

            // wait_readable waits socket until to be readable using select function or until timeout
            error::Error wait_readable(std::uint32_t sec, std::uint32_t usec);
            // wait_readable waits socket until to be writable using select function or until timeout
            error::Error wait_writable(std::uint32_t sec, std::uint32_t usec);

            constexpr explicit operator bool() const {
                return sock != ~0;
            }

            error::Error shutdown(int mode = 2 /*= both*/);

            // get_option invokes getsockopt with getsockopt(layer,opt,std::addressof(t),sizeof(t))
            template <class T>
            error::Error get_option(int layer, int opt, T& t) {
                return get_option(layer, opt, std::addressof(t), sizeof(t));
            }

            // get_option invokes getsockopt with setsockopt(layer,opt,std::addressof(t),sizeof(t))
            template <class T>
            error::Error set_option(int layer, int opt, T&& t) {
                return set_option(layer, opt, std::addressof(t), sizeof(t));
            }

            // set_reuse_addr sets SO_REUSEADDR
            // if this is true,
            // on linux,   you can bind address in TIME_WAIT,CLOSE_WAIT,FIN_WAIT2 (this is windows default behaviour)
            // on windows, you can bind some sockets on same address, but
            // this behaviour has some security risks
            // see also https://learn.microsoft.com/en-us/windows/win32/winsock/using-so-reuseaddr-and-so-exclusiveaddruse
            // Japanese Docs: https://www.ymnet.org/diary/d/%E6%97%A5%E8%A8%98%E3%81%BE%E3%81%A8%E3%82%81/SO_EXCLUSIVEADDRUSE
            error::Error set_reuse_addr(bool resue);

            // set_exclusive_use sets SO_EXCLUSIVEUSE
            // this function works on windows.
            // see also set_reuse_addr behaviour description link
            // on linux, this function always return false
            error::Error set_exclusive_use(bool exclusive);

            // set_ipv6only sets IPV6_V6ONLY
            // if this is false,you can accept both ipv6 and ipv4
            // default value is different between linux(false) and windows(true)
            error::Error set_ipv6only(bool only);
            error::Error set_nodelay(bool no_delay);
            error::Error set_ttl(unsigned char ttl);
            error::Error set_mtu_discover(MTUConfig conf);
            error::Error set_mtu_discover_v6(MTUConfig conf);
            std::int32_t get_mtu();

            // get bound address family,socket type,protocol
            std::tuple<int, int, int, error::Error> get_af_type_protocol();

            // these function sets DF flag on IP layer
            // these function is enable on windows
            // user on linux platform has to use set_mtu_discover(MTUConfig::enable_mtu) instead
            error::Error set_dontfragment(bool df);
            error::Error set_dontfragment_v6(bool df);

            // set_blocking calls ioctl(FIONBIO)
            [[deprecated]] void set_blocking(bool blocking);

            error::Error set_connreset(bool enable);

            // set_skipnotif sets SetFileCompletionNotificationModes()
            // this works on windows
            // this is usable for TCP iocp but UDP is unsafe
            // see also comment of https://docs.microsoft.com/en-us/archive/blogs/winserverperformance/designing-applications-for-high-performance-part-iii
            // if buffer is always over 1500, this may work on UDP
            error::Error set_skipnotify(bool skip_notif, bool skip_event = true);

            // read_until_block calls read function until read returns false
            // callback is void(size_t)
            // if something is read, red would be true
            // otherwise false
            // read_until_block returns block() function result
            error::Error read_until_block(bool& red, view::wvec data, auto&& callback) {
                red = false;
                error::Error err;
                while (true) {
                    view::wvec red_v;
                    std::tie(red_v, err) = read(data);
                    if (err) {
                        break;
                    }
                    callback(red_v.size());
                    red = true;
                }
                if (isSysBlock(err)) {
                    return error::none;
                }
                return err;
            }

            error::Error read_async(size_t bufsize, void* fnctx, void (*cb)(void*, view::wvec data, bool full, error::Error err), int flag = 0, bool is_stream = true);
            error::Error readfrom_async(size_t bufsize, void* fnctx, void (*cb)(void*, view::wvec data, bool truncated, error::Error err, NetAddrPort&& addrport), int flag = 0, bool is_stream = false);
            error::Error write_async(view::rvec src, void* fnctx, void (*cb)(void*, size_t, error::Error err), int flag = 0);
            error::Error writeto_async(view::rvec src, const NetAddrPort& addr, void* fnctx, void (*cb)(void*, size_t, error::Error err), int flag = 0);
            error::Error connect_async(const NetAddrPort& addr, void* fnctx, void (*cb)(void*, error::Error));
            error::Error accept_async(void* fnctx, void (*cb)(void*, Socket, NetAddrPort, error::Error));

            error::Error read_async(auto&& fn, auto&& obj, auto&& get_sock, auto&& add_data) {
                auto fctx = wrap_tuple(std::forward<decltype(fn)>(fn),
                                       std::forward<decltype(obj)>(obj),
                                       std::forward<decltype(get_sock)>(get_sock),
                                       std::forward<decltype(add_data)>(add_data));
                if (!fctx) {
                    return error::memory_exhausted;
                }
                auto r = helper::defer([&] {
                    delete_with_global_heap(fctx, DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(decltype(*fctx)), alignof(decltype(*fctx))));
                });
                Socket& s = std::get<2>(fctx->fn)(std::get<1>(fctx->fn));
                error::Error res = s.read_async(2000, fctx, [](void* p, view::wvec b, bool full, error::Error err) {
                    auto f = decltype(fctx)(p);
                    const auto r = helper::defer([&] {
                        delete_with_global_heap(f, DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(decltype(*f)), alignof(decltype(*f))));
                    });
                    auto& obj = std::get<1>(f->fn);
                    auto& get = std::get<2>(f->fn);
                    auto& add = std::get<3>(f->fn);
                    Socket& s = get(obj);
                    add(obj, (const char*)b.as_char(), b.size());
                    if (!err && full) {
                        bool red = false;
                        s.read_until_block(red, b, [&](size_t redsize) {
                            add(obj, (const char*)b.as_char(), redsize);
                        });
                    }
                    (void)std::get<0>(f->fn)(obj, std::move(err));
                });
                if (!res.is_error()) {
                    r.cancel();
                }
                return res;
            }

            error::Error readfrom_async(auto&& fn, auto&& obj, size_t bufsize, auto&& get_sock) {
                auto fctx = wrap_tuple(std::forward<decltype(fn)>(fn),
                                       std::forward<decltype(obj)>(obj),
                                       std::forward<decltype(get_sock)>(get_sock));
                auto r = helper::defer([&] {
                    delete_with_global_heap(fctx, DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(decltype(*fctx)), alignof(decltype(*fctx))));
                });
                Socket& s = std::get<2>(fctx->fn)(std::get<1>(fctx->fn));
                auto res = s.readfrom_async(bufsize, fctx, [](void* p, view::wvec d, bool truncate, error::Error err, NetAddrPort&& addr) {
                    auto f = decltype(fctx)(p);
                    const auto r = helper::defer([&] {
                        delete_with_global_heap(f, DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(decltype(*f)), alignof(decltype(*f))));
                    });
                    auto& obj = std::get<1>(f->fn);
                    auto& get = std::get<2>(f->fn);
                    Socket& sock = get(obj);
                    // sock.set_err(err);
                    (void)std::get<0>(f->fn)(obj, d, std::move(addr), truncate, std::move(err));
                });
                if (!res.is_error()) {
                    r.cancel();
                }
                return res;
            }
        };

        // make_socket creates socket object
        // before this function calls
        // you mustn't call any other functions
        // if you call any function
        // that is undefined behaviour
        // socket is always non-blocking
        // if you want blocking socket, call Socket::set_blocking explicit
        dnet_dll_export(Socket) make_socket(int address_family, int socket_type, int protocol);

        inline Socket make_socket(SockAttr attr) {
            return make_socket(attr.address_family, attr.socket_type, attr.protocol);
        }

    }  // namespace dnet
}  // namespace utils
