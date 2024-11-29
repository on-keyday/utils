/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
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
#include "event/io.h"
#include "async.h"

namespace futils {
    namespace fnet {

                enum MTUConfig {
            mtu_default,  // same as IP_PMTUDISC_WANT
            mtu_enable,   // same as IP_PMTUDISC_DO, DF flag set on ip layer
            mtu_disable,  // same as IP_PMTUDISC_DONT
            mtu_ignore,   // same as IP_PMTUDISC_PROBE
        };

        // returns whether err means system blocked (ex: non-blocking socket has no data to read)
        fnet_dll_export(bool) isSysBlock(const error::Error& err);

        fnet_dll_export(bool) isMsgSize(const error::Error& err);

        struct Socket;

        enum class ShutdownMode {
            send,
            recv,
            both,
        };

        struct SetDFError {
            error::Error errv4;
            error::Error errv6;

            constexpr void error(auto&& pb) {
                strutil::append(pb, "set DF error: ipv4=");
                errv4.error(pb);
                strutil::append(pb, " ipv6=");
                errv6.error(pb);
            }
        };

        namespace internal {
            template <class A = AsyncResult, class Heap>
            auto async_error_chain(Heap& data_ptr, bool& no_del_if_error) {
                return [&](error::Error&& err) -> expected<A> {
                    const auto d = helper::defer([&] {
                        if (!no_del_if_error) {
                            data_ptr->del(data_ptr);
                        }
                    });
                    return unexpect(std::move(err));
                };
            }

            template <class Heap, class Soc>
            auto done_run_chain(Soc* self, Heap& data_ptr, auto& lambda) {
                return [&, self](AsyncResult&& r) -> expected<AsyncResult> {
                    if (r.state == NotifyState::done) {
                        lambda(self->clone(), data_ptr, r.processed_bytes);
                    }
                    return std::move(r);
                };
            }

            template <class Heap, class Soc>
            auto done_run_with_addr_chain(Soc* self, Heap& data_ptr, auto& lambda) {
                return [&, self](AsyncResult&& r) -> expected<AsyncResult> {
                    if (r.state == NotifyState::done) {
                        lambda(self->clone(), std::move(data_ptr->address), data_ptr, r.processed_bytes);
                    }
                    return std::move(r);
                };
            }

            template <class Heap, class Soc>
            auto done_run_chain_accept(Soc* self, Heap& data_ptr, auto& lambda) {
                return [&, self](AcceptAsyncResult<Soc>&& r) -> expected<AcceptAsyncResult<Soc>> {
                    if (r.state == NotifyState::done) {
                        lambda(self->clone(), std::move(r.socket), std::move(data_ptr->address), data_ptr, std::nullopt);
                    }
                    return std::move(r);
                };
            }

            template <class Soc, class HeapData>
            auto callback_lambda() {
                return +[](Soc&& socket, void* c, NotifyResult&& result) {
                    HeapData* data_ptr = static_cast<HeapData*>(c);
                    const auto d = helper::defer([&] {
                        data_ptr->del(data_ptr);
                    });
                    (data_ptr->fn)(std::move(socket), data_ptr->buffer_mgr, std::move(result));
                };
            }

            template <class Soc, class HeapData>
            auto deferred_callback_lambda() {
                return +[](Soc&& socket, void* ptr, NotifyResult&& result) {
                    HeapData* data_ptr = static_cast<HeapData*>(ptr);
                    const auto deferred = [](void* ptr, bool del_only) {
                        auto* data_ptr = static_cast<HeapData*>(ptr);
                        const auto d = helper::defer([&] {
                            data_ptr->del(data_ptr);
                        });
                        if (!del_only) {
                            (data_ptr->fn)(std::move(data_ptr->socket), data_ptr->buffer_mgr, std::move(data_ptr->result));
                        }
                    };
                    data_ptr->socket = std::move(socket);
                    data_ptr->result = std::move(result);
                    data_ptr->notify(make_deferred_callback(data_ptr, deferred));
                };
            }

            template <class Soc, class HeapData>
            auto callback_lambda_with_addr() {
                return +[](Soc&& socket, NetAddrPort&& addr, void* c, NotifyResult&& result) {
                    HeapData* data_ptr = static_cast<HeapData*>(c);
                    const auto d = helper::defer([&] {
                        data_ptr->del(data_ptr);
                    });
                    (data_ptr->fn)(std::move(socket), data_ptr->buffer_mgr, std::move(addr), std::move(result));
                };
            }

            template <class Soc, class HeapData>
            auto deferred_callback_lambda_with_addr() {
                return +[](Soc&& socket, NetAddrPort&& addr, void* c, NotifyResult&& result) {
                    HeapData* data_ptr = static_cast<HeapData*>(c);
                    const auto deferred = +[](void* ptr, bool del_only) {
                        auto* data_ptr = static_cast<HeapData*>(ptr);
                        const auto d = helper::defer([&] {
                            data_ptr->del(data_ptr);
                        });
                        if (!del_only) {
                            (data_ptr->fn)(std::move(data_ptr->socket), data_ptr->buffer_mgr, std::move(data_ptr->address), std::move(data_ptr->result));
                        }
                    };
                    data_ptr->socket = std::move(socket);
                    data_ptr->result = std::move(result);
                    data_ptr->address = std::move(addr);
                    data_ptr->notify(make_deferred_callback(data_ptr, deferred));
                };
            }

            template <class Soc, class HeapData>
            auto callback_lambda_connect() {
                return +[](Soc&& socket, void* c, NotifyResult&& result) {
                    auto data_ptr = static_cast<HeapData*>(c);
                    const auto d = helper::defer([&] {
                        data_ptr->del(data_ptr);
                    });
                    (data_ptr->fn)(std::move(socket), std::move(result));
                };
            }

            template <class Soc, class HeapData>
            auto deferred_callback_lambda_connect() {
                return +[](Soc&& socket, void* c, NotifyResult&& result) {
                    auto data_ptr = static_cast<HeapData*>(c);
                    const auto deferred = +[](void* ptr, bool del_only) {
                        auto data_ptr = static_cast<HeapData*>(ptr);
                        const auto d = helper::defer([&] {
                            data_ptr->del(data_ptr);
                        });
                        if (!del_only) {
                            (data_ptr->fn)(std::move(data_ptr->socket), std::move(data_ptr->result));
                        }
                    };
                    data_ptr->socket = std::move(socket);
                    data_ptr->result = std::move(result);
                    data_ptr->notify(make_deferred_callback(data_ptr, deferred));
                };
            }

            template <class Soc, class HeapData>
            auto callback_lambda_accept() {
                return +[](Soc&& listener, Soc&& accepted, NetAddrPort&& addr, void* c, NotifyResult&& result) {
                    auto data_ptr = static_cast<HeapData*>(c);
                    const auto d = helper::defer([&] {
                        data_ptr->del(data_ptr);
                    });
                    (data_ptr->fn)(std::move(listener), std::move(accepted), std::move(addr), std::move(result));
                };
            }

            template <class Soc, class HeapData>
            auto deferred_callback_lambda_accept() {
                return +[](Soc&& listener, Soc&& accepted, NetAddrPort&& addr, void* c, NotifyResult&& result) {
                    auto data_ptr = static_cast<HeapData*>(c);
                    const auto deferred = [](void* ptr, bool del_only) {
                        auto* data_ptr = static_cast<HeapData*>(ptr);
                        const auto d = helper::defer([&] {
                            data_ptr->del(data_ptr);
                        });
                        if (!del_only) {
                            (data_ptr->fn)(std::move(data_ptr->socket), std::move(data_ptr->accepted), std::move(data_ptr->address), std::move(data_ptr->result));
                        }
                    };
                    data_ptr->socket = std::move(listener);
                    data_ptr->accepted = std::move(accepted);
                    data_ptr->result = std::move(result);
                    data_ptr->address = std::move(addr);
                    data_ptr->notify(make_deferred_callback(data_ptr, deferred));
                };
            }
        }  // namespace internal

        struct KeepAliveParameter {
            bool enable = false;
            std::uint32_t idle = 0;
            std::uint32_t interval = 0;
            std::uint32_t count = 0;
        };

        // Socket is wrapper class of native socket
        struct fnet_class_export Socket {
           private:
            void* ctx = nullptr;

            constexpr Socket(void* c)
                : ctx(c) {}
            expected<void> get_option(int layer, int opt, void* buf, size_t size);
            expected<void> set_option(int layer, int opt, const void* buf, size_t size);
            friend Socket make_socket(void* uptr);

            expected<std::uintptr_t> get_raw();

           public:
            constexpr Socket() = default;
            ~Socket();
            constexpr Socket(Socket&& sock) noexcept
                : ctx(std::exchange(sock.ctx, nullptr)) {}

            // explicit clone
            // in default case, user should use one instance of socket,
            // but some case, for example async operation, requires explicit clone
            // this is NOT a dup() or DuplicateHandle(), only increment reference count
            Socket clone() const;

            Socket& operator=(Socket&& sock) noexcept {
                if (this == &sock) {
                    return *this;
                }
                this->~Socket();
                this->ctx = std::exchange(sock.ctx, nullptr);
                return *this;
            }

            constexpr explicit operator bool() const {
                return ctx != nullptr;
            }

            // connection management

            [[nodiscard]] expected<std::pair<Socket, NetAddrPort>> accept();

            // use select before call accept
            [[nodiscard]] expected<std::pair<Socket, NetAddrPort>> accept_select(std::uint32_t sec, std::uint32_t usec) {
                return wait_readable(sec, usec)
                    .and_then([&] {
                        return accept();
                    });
            }

            expected<void> bind(const NetAddrPort& addr);

            expected<void> listen(int backlog = 10);

            expected<void> connect(const NetAddrPort& addr);

            expected<void> shutdown(ShutdownMode mode = ShutdownMode::both);

            // I/O methods

            // returns remaining data
            expected<view::rvec> write(view::rvec data, int flag = 0);

            // returns read bytes
            expected<view::wvec> read(view::wvec data, int flag = 0, bool is_stream = true);

            // returns remaining data
            expected<view::rvec> writeto(const NetAddrPort& addr, view::rvec data, int flag = 0);

            // returns read bytes and address
            expected<std::pair<view::wvec, NetAddrPort>> readfrom(view::wvec data, int flag = 0, bool is_stream = false);

            // returns read bytes and address
            expected<view::rvec> writemsg(const NetAddrPort& addr, SockMsg<view::rvec> msg, int flag = 0);

            // returns read bytes and address
            expected<std::pair<view::wvec, NetAddrPort>> readmsg(SockMsg<view::wvec> msg, int flag = 0, bool is_stream = true);

            // wait_readable waits socket until to be readable using select function or until timeout
            expected<void> wait_readable(std::uint32_t sec, std::uint32_t usec);
            // wait_readable waits socket until to be writable using select function or until timeout
            expected<void> wait_writable(std::uint32_t sec, std::uint32_t usec);

            // The read_until_block function repeatedly calls the read function
            // until it encounters a blocking condition (e.g., EAGAIN or EWOULDBLOCK).
            // If it encounters EOF (End Of File), it treats it as an error and returns it.
            //
            // Parameters:
            // - callback: A function of type void(view::rvec) that processes the received data.
            // - data: Optional; a writable buffer (view::wvec) for receiving data. If empty,
            //         an internal buffer of 1024 bytes is used.
            //
            // Return value:
            // - Returns the total size of data successfully read, or an error (e.g., error::eof).
            // - If it returns 0, it indicates no data was available to read,
            //   and waiting for an IO event (e.g., with epoll_wait) is appropriate.
            //
            // Error handling:
            // - If the `read` function encounters EOF (End Of File), it returns an error,
            //   which is treated as an error condition in `read_until_block`.
            // - If the socket is non-blocking and `read` returns EAGAIN or EWOULDBLOCK,
            //   the function will return the size of data read so far and allow waiting for further events.
            expected<size_t> read_until_block(auto&& callback, view::wvec data = {}) {
                auto do_read = [&]() -> expected<size_t> {
                    size_t size = 0;
                    while (true) {
                        auto recv = read(data);
                        if (!recv) {
                            if (isSysBlock(recv.error())) {
                                return size;  // as no error
                            }
                            return recv.transform([&](view::wvec) { return size; });
                        }
                        size += recv->size();
                        callback(*recv);
                    }
                };
                if (data.empty()) {
                    byte buf[1024];
                    data = view::wvec(buf, 1024);
                    return do_read();
                }
                return do_read();
            }

            expected<size_t> readfrom_until_block(view::wvec data, auto&& callback) {
                bool red = false;
                error::Error err;
                size_t size = 0;
                while (true) {
                    auto recv = readfrom(data);
                    if (!recv) {
                        if (isSysBlock(recv.error())) {
                            return size;  // as no error
                        }
                        return recv.transform([&](std::pair<view::wvec, NetAddrPort>) { return size; });
                    }
                    size += recv->first.size();
                    callback(recv->first, recv->second);
                    red = true;
                }
            }

            // get/set attributes of socket

            expected<NetAddrPort> get_local_addr();

            expected<NetAddrPort> get_remote_addr();

            // get_option invokes getsockopt with getsockopt(layer,opt,std::addressof(t),sizeof(t))
            template <class T>
            expected<void> get_option(int layer, int opt, T& t) {
                return get_option(layer, opt, std::addressof(t), sizeof(t));
            }

            // get_option invokes getsockopt with setsockopt(layer,opt,std::addressof(t),sizeof(t))
            template <class T>
            expected<void> set_option(int layer, int opt, T&& t) {
                return set_option(layer, opt, std::addressof(t), sizeof(t));
            }

            // set_reuse_addr sets SO_REUSEADDR
            // if this is true,
            // on linux,   you can bind address in TIME_WAIT,CLOSE_WAIT,FIN_WAIT2 (this is windows default behaviour)
            // on windows, you can bind some sockets on same address, but
            // this behaviour has some security risks
            // see also https://learn.microsoft.com/en-us/windows/win32/winsock/using-so-reuseaddr-and-so-exclusiveaddruse
            // Japanese Docs: https://www.ymnet.org/diary/d/%E6%97%A5%E8%A8%98%E3%81%BE%E3%81%A8%E3%82%81/SO_EXCLUSIVEADDRUSE
            expected<void> set_reuse_addr(bool resue);

            // set_exclusive_use sets SO_EXCLUSIVEUSE
            // this function works on windows.
            // see also set_reuse_addr behaviour description link
            // on linux, this function always return false
            expected<void> set_exclusive_use(bool exclusive);

            // set_ipv6only sets IPV6_V6ONLY
            // if this is false,you can accept both ipv6 and ipv4
            // default value is different between linux(false) and windows(true)
            expected<void> set_ipv6only(bool only);
            expected<void> set_nodelay(bool no_delay);
            expected<void> set_ttl(unsigned char ttl);
            expected<void> set_mtu_discover(MTUConfig conf);
            expected<void> set_mtu_discover_v6(MTUConfig conf);
            expected<std::int32_t> get_mtu();

            // get bound address family,socket type,protocol
            expected<SockAttr> get_sockattr();

            // these function sets DF flag on IP layer
            // these function is enable on windows
            // user on linux platform has to use set_mtu_discover(MTUConfig::enable_mtu) instead
            expected<void> set_dont_fragment_v4(bool df);
            expected<void> set_dont_fragment_v6(bool df);

            // set_DF is commonly used to set IP layer Dont Fragment flag
            // this choose best way to set DF flag in each platform
            expected<void> set_DF(bool df);

            // for raw socket
            expected<void> set_header_include_v4(bool incl);
            expected<void> set_header_include_v6(bool incl);

            // set_blocking calls ioctl(FIONBIO)
            [[deprecated]] void set_blocking(bool blocking);

            // control SIO_UDP_CONNRESET
            // this enable on windows
            expected<void> set_connreset(bool enable);

            // set buffer size
            expected<void> set_send_buffer_size(size_t size);
            expected<void> set_recv_buffer_size(size_t size);

            // get buffer size
            expected<size_t> get_send_buffer_size();
            expected<size_t> get_recv_buffer_size();

            // GSO is Generic Segmentation Offload
            // this is linux only
            // see also https://www.kernel.org/doc/html/latest/networking/gso.html
            expected<void> set_gso(bool enable);
            expected<bool> get_gso();

            // set_packet_info sets IP_PKTINFO
            expected<void> set_recv_packet_info_v4(bool enable);
            expected<bool> get_recv_packet_info_v4();
            // set_packet_info_v6 sets IPV6_RECVPKTINFO
            expected<void> set_recv_packet_info_v6(bool enable);
            expected<bool> get_recv_packet_info_v6();

            // for ipv4
            expected<void> set_recv_tos(bool enable);
            expected<bool> get_recv_tos();

            // for ipv6
            expected<void> set_recv_tclass(bool enable);
            expected<bool> get_recv_tclass();

            // set_skipnotif sets SetFileCompletionNotificationModes()
            // this works on windows
            // this is usable for TCP iocp but UDP is unsafe
            // see also comment of https://docs.microsoft.com/en-us/archive/blogs/winserverperformance/designing-applications-for-high-performance-part-iii
            // if buffer is always over 1500, this may work on UDP
            expected<void> set_skipnotify(bool skip_notif, bool skip_event = true);

            // tcp keep alive
            expected<KeepAliveParameter> get_tcp_keep_alive();
            expected<void> set_tcp_keep_alive(const KeepAliveParameter& param);

            // async I/O methods
            // caller is responsible for buffer management until completion

            expected<AsyncResult> read_async(view::wvec buffer, void* c, stream_notify_t notify, std::uint32_t flag = 0);
            expected<AsyncResult> write_async(view::rvec buffer, void* c, stream_notify_t notify, std::uint32_t flag = 0);
            expected<AsyncResult> readfrom_async(view::wvec buffer, NetAddrPort& addr, void* c, recvfrom_notify_t notify, std::uint32_t flag = 0);
            expected<AsyncResult> writeto_async(view::rvec buffer, const NetAddrPort& addr, void* c, stream_notify_t notify, std::uint32_t flag = 0);

            expected<AsyncResult> connect_async(const NetAddrPort& addr, void* c, stream_notify_t notify, std::uint32_t flag = 0);
            // connect_ex_loopback_optimize is optimization of connect_async for loopback address
            // this works on windows, and this is nop on linux
            expected<void> connect_ex_loopback_optimize(const NetAddrPort& addr);
            expected<AcceptAsyncResult<Socket>> accept_async(NetAddrPort& addr, void* c, accept_notify_t notify, std::uint32_t flag = 0);

            // async I/O methods with lambda
            // these are wrappers of above

            template <class Fn, class BufMgr, class Del>
            expected<AsyncResult> read_async(AsyncContBufData<Fn, BufMgr, Del>* data_ptr, bool no_del_if_error = false, std::uint32_t flag = 0) {
                if (!data_ptr) {
                    return unexpect("data_ptr must not be null", error::Category::lib, error::fnet_usage_error);
                }

                using HeapData = AsyncContBufData<Fn, BufMgr, Del>;

                static_assert(std::is_same_v<HeapData, std::decay_t<decltype(*data_ptr)>>);

                auto lambda = internal::callback_lambda<Socket, HeapData>();

                return read_async(data_ptr->buffer_mgr.get_buffer(), data_ptr, lambda, flag)
                    .and_then(internal::done_run_chain(this, data_ptr, lambda))
                    .or_else(internal::async_error_chain(data_ptr, no_del_if_error));
            }

            template <class Notify, class Fn, class BufMgr, class Del>
            expected<AsyncResult> read_async_deferred(AsyncContNotifyBufData<Fn, BufMgr, Del, Notify>* data_ptr, bool no_del_if_error = false, std::uint32_t flag = 0) {
                if (!data_ptr) {
                    return unexpect("data_ptr must not be null", error::Category::lib, error::fnet_usage_error);
                }

                using HeapData = AsyncContNotifyBufData<Fn, BufMgr, Del, Notify>;

                static_assert(std::is_same_v<HeapData, std::decay_t<decltype(*data_ptr)>>);

                auto lambda = internal::deferred_callback_lambda<Socket, HeapData>();

                return read_async(data_ptr->buffer_mgr.get_buffer(), data_ptr, lambda, flag)
                    .and_then(internal::done_run_chain(this, data_ptr, lambda))
                    .or_else(internal::async_error_chain(data_ptr, no_del_if_error));
            }

            template <class Fn, class BufMgr, class Del>
            expected<AsyncResult> write_async(AsyncContBufData<Fn, BufMgr, Del>* data_ptr, bool no_del_if_error = false, std::uint32_t flag = 0) {
                if (!data_ptr) {
                    return unexpect("data_ptr must not be null", error::Category::lib, error::fnet_usage_error);
                }

                using HeapData = AsyncContBufData<Fn, BufMgr, Del>;

                static_assert(std::is_same_v<HeapData, std::decay_t<decltype(*data_ptr)>>);

                auto lambda = internal::callback_lambda<Socket, HeapData>();

                return write_async(data_ptr->buffer_mgr.get_buffer(), data_ptr, +lambda, flag)
                    .and_then(internal::done_run_chain(this, data_ptr, lambda))
                    .or_else(internal::async_error_chain(data_ptr, no_del_if_error));
            }

            template <class Notify, class Fn, class BufMgr, class Del>
            expected<AsyncResult> write_async_deferred(AsyncContNotifyBufData<Fn, BufMgr, Del, Notify>* data_ptr, bool no_del_if_error = false, std::uint32_t flag = 0) {
                if (!data_ptr) {
                    return unexpect("data_ptr must not be null", error::Category::lib, error::fnet_usage_error);
                }

                using HeapData = AsyncContNotifyBufData<Fn, BufMgr, Del, Notify>;

                static_assert(std::is_same_v<HeapData, std::decay_t<decltype(*data_ptr)>>);

                auto lambda = internal::deferred_callback_lambda<Socket, HeapData>();

                return write_async(data_ptr->buffer_mgr.get_buffer(), data_ptr, +lambda, flag)
                    .and_then(internal::done_run_chain(this, data_ptr, lambda))
                    .or_else(internal::async_error_chain(data_ptr, no_del_if_error));
            }

            template <class Fn, class BufMgr, class Del>
            expected<AsyncResult> readfrom_async(AsyncContBufAddrData<Fn, BufMgr, Del>* data_ptr, bool no_del_if_error = false, std::uint32_t flag = 0) {
                if (!data_ptr) {
                    return unexpect("data_ptr must not be null", error::Category::lib, error::fnet_usage_error);
                }

                using HeapData = AsyncContBufAddrData<Fn, BufMgr, Del>;

                static_assert(std::is_same_v<HeapData, std::decay_t<decltype(*data_ptr)>>);

                auto lambda = internal::callback_lambda_with_addr<Socket, HeapData>();

                return readfrom_async(data_ptr->buffer_mgr.get_buffer(), data_ptr->address, data_ptr, +lambda, flag)
                    .and_then(internal::done_run_with_addr_chain(this, data_ptr, lambda))
                    .or_else(internal::async_error_chain(data_ptr, no_del_if_error));
            }

            template <class Notify, class Fn, class BufMgr, class Del>
            expected<AsyncResult> readfrom_async_deferred(AsyncContNotifyBufAddrData<Fn, BufMgr, Del, Notify>* data_ptr, bool no_del_if_error = false, std::uint32_t flag = 0) {
                if (!data_ptr) {
                    return unexpect("data_ptr must not be null", error::Category::lib, error::fnet_usage_error);
                }

                using HeapData = AsyncContNotifyBufAddrData<Fn, BufMgr, Del, Notify>;

                static_assert(std::is_same_v<HeapData, std::decay_t<decltype(*data_ptr)>>);

                auto lambda = internal::deferred_callback_lambda_with_addr<Socket, HeapData>();

                return readfrom_async(data_ptr->buffer_mgr.get_buffer(), data_ptr->address, data_ptr, +lambda, flag)
                    .and_then(internal::done_run_with_addr_chain(this, data_ptr, lambda))
                    .or_else(internal::async_error_chain(data_ptr, no_del_if_error));
            }

            template <class Fn, class BufMgr, class Del>
            expected<AsyncResult> writeto_async(AsyncContBufAddrData<Fn, BufMgr, Del>* data_ptr, bool no_del_if_error = false, std::uint32_t flag = 0) {
                if (!data_ptr) {
                    return unexpect("data_ptr must not be null", error::Category::lib, error::fnet_usage_error);
                }

                using HeapData = AsyncContBufAddrData<Fn, BufMgr, Del>;

                static_assert(std::is_same_v<HeapData, std::decay_t<decltype(*data_ptr)>>);

                auto lambda = internal::callback_lambda<Socket, HeapData>();

                return writeto_async(data_ptr->buffer_mgr.get_buffer(), data_ptr->address, data_ptr, +lambda, flag)
                    .and_then(internal::done_run_chain(this, data_ptr, lambda))
                    .or_else(internal::async_error_chain(data_ptr, no_del_if_error));
            }

            template <class Notify, class Fn, class BufMgr, class Del>
            expected<AsyncResult> writeto_async_deferred(AsyncContNotifyBufAddrData<Fn, BufMgr, Del, Notify>* data_ptr, bool no_del_if_error = false, std::uint32_t flag = 0) {
                if (!data_ptr) {
                    return unexpect("data_ptr must not be null", error::Category::lib, error::fnet_usage_error);
                }

                using HeapData = AsyncContNotifyBufData<Fn, BufMgr, Del, Notify>;

                static_assert(std::is_same_v<HeapData, std::decay_t<decltype(*data_ptr)>>);

                auto lambda = internal::deferred_callback_lambda<Socket, HeapData>();

                return writeto_async(data_ptr->buffer_mgr.get_buffer(), data_ptr->address, data_ptr, +lambda, flag)
                    .and_then(internal::done_run_chain(this, data_ptr, lambda))
                    .or_else(internal::async_error_chain(data_ptr, no_del_if_error));
            }

            template <class Fn, class Del>
            expected<AcceptAsyncResult<Socket>> accept_async(AsyncContAddrData<Fn, Del>* data_ptr, bool no_del_if_error = false, std::uint32_t flag = 0) {
                if (!data_ptr) {
                    return unexpect("data_ptr must not be null", error::Category::lib, error::fnet_usage_error);
                }

                using HeapData = AsyncContAddrData<Fn, Del>;

                auto lambda = internal::callback_lambda_accept<Socket, HeapData>();

                return accept_async(data_ptr->address, data_ptr, lambda, flag)
                    .and_then(internal::done_run_chain_accept(this, data_ptr, lambda))
                    .or_else(internal::async_error_chain<AcceptAsyncResult<Socket>>(data_ptr, no_del_if_error));
            }

            template <class Notify, class Fn, class Del>
            expected<AcceptAsyncResult<Socket>> accept_async_deferred(AsyncContNotifyAddrDataAccept<Fn, Del, Notify>* data_ptr, bool no_del_if_error = false, std::uint32_t flag = 0) {
                if (!data_ptr) {
                    return unexpect("data_ptr must not be null", error::Category::lib, error::fnet_usage_error);
                }

                using HeapData = AsyncContNotifyAddrDataAccept<Fn, Del, Notify>;

                auto lambda = internal::deferred_callback_lambda_accept<Socket, HeapData>();

                return accept_async(data_ptr->address, data_ptr, lambda, flag)
                    .and_then(internal::done_run_chain_accept(this, data_ptr, lambda))
                    .or_else(internal::async_error_chain<AcceptAsyncResult<Socket>>(data_ptr, no_del_if_error));
            }

            template <class Fn, class Del>
            expected<AsyncResult> connect_async(AsyncContAddrData<Fn, Del>* data_ptr, bool no_del_if_error = false, std::uint32_t flag = 0) {
                if (!data_ptr) {
                    return unexpect("data_ptr must not be null", error::Category::lib, error::fnet_usage_error);
                }

                using HeapData = AsyncContAddrData<Fn, Del>;

                auto lambda = internal::callback_lambda_connect<Socket, HeapData>();

                return connect_async(data_ptr->address, data_ptr, lambda, flag)
                    .and_then(internal::done_run_chain(this, data_ptr, lambda))
                    .or_else(internal::async_error_chain(data_ptr, no_del_if_error));
            }

            template <class Notify, class Fn, class Del>
            expected<AsyncResult> connect_async_deferred(AsyncContNotifyAddrData<Fn, Del, Notify>* data_ptr, bool no_del_if_error = false, std::uint32_t flag = 0) {
                if (!data_ptr) {
                    return unexpect("data_ptr must not be null", error::Category::lib, error::fnet_usage_error);
                }

                using HeapData = AsyncContNotifyAddrData<Fn, Del, Notify>;

                auto lambda = internal::deferred_callback_lambda_connect<Socket, HeapData>();

                return connect_async(data_ptr->address, data_ptr, lambda, flag)
                    .and_then(internal::done_run_chain(this, data_ptr, lambda))
                    .or_else(internal::async_error_chain(data_ptr, no_del_if_error));
            }
        };

        // make_socket creates socket object
        // before this function calls
        // you mustn't call any other functions
        // if you call any function
        // that is undefined behaviour
        // socket is always non-blocking
        // if you want blocking socket, call Socket::set_blocking explicit
        // io is IOEvent that handles async IO
        // caller must guarantee io is alive while Socket is alive
        // if io is nullptr, using default IOEvent that uses event::fnet_handle_completion directly
        fnet_dll_export(expected<Socket>) make_socket(SockAttr attr, event::IOEvent* io = nullptr);

    }  // namespace fnet
}  // namespace futils
