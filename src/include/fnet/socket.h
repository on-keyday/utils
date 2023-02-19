/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// socket - native socket interface
#pragma once
#include <cstdint>
#include <cstddef>
#include <utility>
#include <memory>
#include "dll/dllh.h"
#include "heap.h"
#include "dll/glheap.h"
#include "address.h"
#include "error.h"
#include "../thread/waker.h"
#include "storage.h"

namespace utils {
    namespace fnet {

        // wait_event waits io completion until time passed
        // if event is processed function returns number of event
        // otherwise returns 0
        // this call GetQueuedCompletionStatusEx on windows
        // or call epoll_wait on linux
        fnet_dll_export(int) wait_io_event(std::uint32_t time);

        enum MTUConfig {
            mtu_default,  // same as IP_PMTUDISC_WANT
            mtu_enable,   // same as IP_PMTUDISC_DO, DF flag set on ip layer
            mtu_disable,  // same as IP_PMTUDISC_DONT
            mtu_ignore,   // same as IP_PMTUDISC_PROBE
        };

        fnet_dll_export(bool) isSysBlock(const error::Error& err);
        fnet_dll_export(bool) isMsgSize(const error::Error& err);
        struct Socket;

        namespace internal {

            struct WakerArg {
                size_t size = 0;
                error::Error err;
                NetAddrPort addr;
            };

            struct WakerAcceptArg {
                Socket* new_socket;
                NetAddrPort addr;
            };
        }  // namespace internal

        struct SockCanceler {
           private:
            friend struct Socket;
            std::shared_ptr<thread::Waker> waker;

            SockCanceler(std::shared_ptr<thread::Waker>&& w)
                : waker(std::move(w)) {}

           public:
            constexpr SockCanceler() = default;

            bool cancel() {
                return waker && waker->cancel();
            }
        };

        struct NullFunc {
            constexpr void operator()(auto&&...) {}
        };

        // Socket is wrappper class of native socket
        struct fnet_class_export Socket {
           private:
            std::uintptr_t sock = ~0;
            void* async_ctx = nullptr;

            constexpr Socket(std::uintptr_t s)
                : sock(s) {}
            error::Error get_option(int layer, int opt, void* buf, size_t size);
            error::Error set_option(int layer, int opt, const void* buf, size_t size);
            friend Socket make_socket(std::uintptr_t uptr);

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

            constexpr explicit operator bool() const {
                return sock != ~0;
            }

            // connection management

            [[nodiscard]] std::tuple<Socket, NetAddrPort, error::Error> accept();

            error::Error bind(const NetAddrPort& addr);

            error::Error listen(int backlog = 10);

            error::Error connect(const NetAddrPort& addr);

            error::Error shutdown(int mode = 2 /*= both*/);

            // I/O methods

            // returns (remain,err)
            std::pair<view::rvec, error::Error> write(view::rvec data, int flag = 0);

            // returns (read,err)
            std::pair<view::wvec, error::Error> read(view::wvec data, int flag = 0, bool is_stream = true);

            // returns (remain,err)
            std::pair<view::rvec, error::Error> writeto(const NetAddrPort& addr, view::rvec data, int flag = 0);

            std::tuple<view::wvec, NetAddrPort, error::Error> readfrom(view::wvec data, int flag = 0, bool is_stream = false);

            // wait_readable waits socket until to be readable using select function or until timeout
            error::Error wait_readable(std::uint32_t sec, std::uint32_t usec);
            // wait_readable waits socket until to be writable using select function or until timeout
            error::Error wait_writable(std::uint32_t sec, std::uint32_t usec);

            // read_until_block calls read function until read returns false
            // callback is void(view::rvec)
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
                    callback(red_v);
                    red = true;
                }
                if (isSysBlock(err)) {
                    return error::none;
                }
                return err;
            }

            // get/set attributes of socket

            std::pair<NetAddrPort, error::Error> get_localaddr();

            std::pair<NetAddrPort, error::Error> get_remoteaddr();

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
            std::pair<SockAttr, error::Error> get_sockattr();

            // these function sets DF flag on IP layer
            // these function is enable on windows
            // user on linux platform has to use set_mtu_discover(MTUConfig::enable_mtu) instead
            error::Error set_dontfragment(bool df);
            error::Error set_dontfragment_v6(bool df);

            // set_blocking calls ioctl(FIONBIO)
            [[deprecated]] void set_blocking(bool blocking);

            // control SIO_UDP_CONNRESET
            // this enable on windows
            error::Error set_connreset(bool enable);

            // set_skipnotif sets SetFileCompletionNotificationModes()
            // this works on windows
            // this is usable for TCP iocp but UDP is unsafe
            // see also comment of https://docs.microsoft.com/en-us/archive/blogs/winserverperformance/designing-applications-for-high-performance-part-iii
            // if buffer is always over 1500, this may work on UDP
            error::Error set_skipnotify(bool skip_notif, bool skip_event = true);

            // async I/O methods

            // waker->wake() should accept internal::WakerArg
            error::Error read_async(view::wvec buf, std::shared_ptr<thread::Waker> waker, int flag = 0, bool is_stream = true);
            // waker->wake() should accept internal::WakerArg
            error::Error readfrom_async(view::wvec buf, std::shared_ptr<thread::Waker> waker, int flag = 0, bool is_stream = false);
            // waker->wake() should accept internal::WakerArg
            error::Error write_async(view::rvec src, std::shared_ptr<thread::Waker> waker, int flag = 0);
            // waker->wake() should accept internal::WakerArg
            error::Error writeto_async(view::rvec src, const NetAddrPort& addr, std::shared_ptr<thread::Waker> waker, int flag = 0);
            // waker->wake() should accept internal::WakerArg
            error::Error connect_async(const NetAddrPort& addr, std::shared_ptr<thread::Waker> waker);
            // waker->wake() should accept internal::WakerAcceptArg
            error::Error accept_async(std::shared_ptr<thread::Waker> waker);

           private:
            bool cancel_io(bool is_read);

            template <class Detail, class Arg = internal::WakerArg>
            std::pair<SockCanceler, error::Error> do_async(
                auto&& buffer, auto&& run, auto&& woken, bool run_on_wake,
                auto&& copy_on_waking, auto&& async_call, bool is_read) {
                struct L {
                    fnet::Socket self;
                    std::decay_t<decltype(buffer)> buffer_;
                    std::decay_t<decltype(run)> run_;
                    std::decay_t<decltype(woken)> woken_;
                    std::decay_t<decltype(copy_on_waking)> copy_;
                    bool run_on_wake_ = false;
                    bool is_read_ = false;
                    bool is_canceled = false;
                    Detail detail;
                    L(Socket&& a, decltype(buffer) b, decltype(run) c, decltype(woken) d, decltype(copy_on_waking) e, bool f, bool g)
                        : self(std::forward<decltype(a)>(a)),
                          buffer_(std::forward<decltype(b)>(b)),
                          run_(std::forward<decltype(c)>(c)),
                          woken_(std::forward<decltype(d)>(d)),
                          copy_(std::forward<decltype(e)>(e)),
                          run_on_wake_(f),
                          is_read_(g) {}
                };
                std::shared_ptr<L> param = std::allocate_shared<L>(
                    glheap_allocator<L>{},
                    std::move(*this),
                    std::forward<decltype(buffer)>(buffer),
                    std::forward<decltype(run)>(run),
                    std::forward<decltype(woken)>(woken),
                    std::forward<decltype(copy_on_waking)>(copy_on_waking),
                    run_on_wake,
                    is_read);
                thread::WakerCallback cb = [](const std::shared_ptr<thread::Waker>& w,
                                              std::shared_ptr<void>& ctx,
                                              thread::WakerState s, void* arg) {
                    L* l = static_cast<L*>(ctx.get());
                    auto run = [&](void* arg) {
                        l->run_(l, arg);
                    };
                    switch (s) {
                        case thread::WakerState::waking: {
                            Arg* warg = static_cast<Arg*>(arg);
                            l->copy_(l, warg);
                            if (l->run_on_wake_) {
                                run(nullptr);
                            }
                            break;
                        }
                        case thread::WakerState::canceling: {
                            if (l->is_canceled) {
                                return 0;  // already succeeded
                            }
                            l->is_canceled = l->self.cancel_io(l->is_read_);
                            return l->is_canceled ? 0 : -1;
                        }
                        case thread::WakerState::woken: {
                            l->woken_(w);
                            break;
                        }
                        case thread::WakerState::running: {
                            if (!l->run_on_wake_) {
                                run(arg);
                            }
                            break;
                        }
                        default:
                            break;
                    }
                    return 1;
                };
                auto waker = thread::allocate_waker(glheap_allocator<thread::Waker>{}, param, cb);
                L* l = param.get();
                error::Error err = async_call(l, waker);
                if (err.is_error()) {
                    *this = std::move(l->self);  // get back
                    return {{}, err};
                }
                return {{std::move(waker)}, error::none};
            }

           public:
            // cb is void(Socket&&,view::wvec read,auto&& buffer,error::Error err,void* arg)
            // woken is void(std::shared_ptr<thread::Waker>)
            // this function transfer ownership of this Socket to async operation system
            // this Socket will be returned by cb
            template <class WokenFn = NullFunc>
            std::pair<SockCanceler, error::Error> read_async(auto&& buffer, auto&& cb, bool run_on_wake = true,
                                                             WokenFn&& woken = NullFunc{}, int flag = 0) {
                struct D {
                    size_t size = 0;
                    error::Error err;
                };
                return do_async<D>(
                    std::forward<decltype(buffer)>(buffer),
                    [cb = std::forward<decltype(cb)>(cb)](auto* l, void* arg) mutable {
                        cb(std::move(l->self),
                           view::wvec(l->buffer_).substr(0, l->detail.size),
                           std::move(l->buffer_),
                           std::move(l->detail.err),
                           arg);
                    },
                    std::forward<decltype(woken)>(woken),
                    run_on_wake,
                    [](auto* l, internal::WakerArg* arg) {
                        l->detail.size = arg->size;
                        l->detail.err = std::move(arg->err);
                    },
                    [&](auto* l, std::shared_ptr<thread::Waker>& w) {
                        Socket& self = l->self;
                        return self.read_async(l->buffer_, w, flag);
                    },
                    true);
            }

            // cb is void(Socket&&,view::wvec read,auto&& buffer,NetAddrPort&& addr,error::Error err,void* arg)
            // woken is void(std::shared_ptr<thread::Waker>)
            template <class WokenFn = NullFunc>
            std::pair<SockCanceler, error::Error> readfrom_async(auto&& buffer, auto&& cb, bool run_on_wake = true,
                                                                 WokenFn&& woken = NullFunc{}, int flag = 0) {
                return do_async<internal::WakerArg>(
                    std::forward<decltype(buffer)>(buffer),
                    [=](auto* l, void* arg) {
                        cb(std::move(l->self),
                           view::wvec(l->buffer_).substr(0, l->detail.size),
                           std::move(l->buffer_),
                           std::move(l->detail.addr),
                           std::move(l->detail.err),
                           arg);
                    },
                    std::forward<decltype(woken)>(woken),
                    run_on_wake,
                    [](auto* l, internal::WakerArg* arg) { l->detail = std::move(*arg); },
                    [&](auto* l, std::shared_ptr<thread::Waker>& w) {
                        Socket& self = l->self;
                        return self.readfrom_async(l->buffer_, w, flag);
                    },
                    true);
            }
        };

        // make_socket creates socket object
        // before this function calls
        // you mustn't call any other functions
        // if you call any function
        // that is undefined behaviour
        // socket is always non-blocking
        // if you want blocking socket, call Socket::set_blocking explicit
        fnet_dll_export(Socket) make_socket(SockAttr attr);

    }  // namespace fnet
}  // namespace utils
