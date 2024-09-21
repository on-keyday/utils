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

        // wait_event waits io completion until time passed
        // if event is processed function returns number of event
        // otherwise returns 0
        // this call GetQueuedCompletionStatusEx on windows
        // or call epoll_wait on linux
        // this is based on default IOEvent
        // so if you uses custom IOEvent, use it's wait method instead
        fnet_dll_export(expected<size_t>) wait_io_event(std::uint32_t time);

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
            constexpr Socket(Socket&& sock)
                : ctx(std::exchange(sock.ctx, nullptr)) {}

            // explicit clone
            // in default case, user should use one instance of socket,
            // but some case, for example async operation, requires explicit clone
            // this is NOT a dup() or DuplicateHandle(), only increment reference count
            Socket clone() const;

            Socket& operator=(Socket&& sock) {
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

            // returns (remain,err)
            expected<view::rvec> write(view::rvec data, int flag = 0);

            // returns (read,err)
            expected<view::wvec> read(view::wvec data, int flag = 0, bool is_stream = true);

            // returns (remain,err)
            expected<view::rvec> writeto(const NetAddrPort& addr, view::rvec data, int flag = 0);

            expected<std::pair<view::wvec, NetAddrPort>> readfrom(view::wvec data, int flag = 0, bool is_stream = false);

            // wait_readable waits socket until to be readable using select function or until timeout
            expected<void> wait_readable(std::uint32_t sec, std::uint32_t usec);
            // wait_readable waits socket until to be writable using select function or until timeout
            expected<void> wait_writable(std::uint32_t sec, std::uint32_t usec);

            // read_until_block calls read function until read returns false
            // callback is void(view::rvec)
            // if something is read, red would be true
            // otherwise false
            // read_until_block returns block() function result
            expected<size_t> read_until_block(view::wvec data, auto&& callback) {
                bool red = false;
                error::Error err;
                size_t size = 0;
                while (true) {
                    auto recv = read(data);
                    if (!recv) {
                        if (isSysBlock(recv.error())) {
                            if (!red) {
                                return recv.transform([&](view::wvec) { return size; });
                            }
                            return size;  // as no error
                        }
                        return recv.transform([&](view::wvec) { return size; });
                    }
                    size += recv->size();
                    callback(*recv);
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

            // set_skipnotif sets SetFileCompletionNotificationModes()
            // this works on windows
            // this is usable for TCP iocp but UDP is unsafe
            // see also comment of https://docs.microsoft.com/en-us/archive/blogs/winserverperformance/designing-applications-for-high-performance-part-iii
            // if buffer is always over 1500, this may work on UDP
            expected<void> set_skipnotify(bool skip_notif, bool skip_event = true);

            // async I/O methods
            // caller is responsible for buffer management until completion

            expected<AsyncResult> read_async(view::wvec buffer, void* c, stream_notify_t notify, std::uint32_t flag = 0);
            expected<AsyncResult> write_async(view::rvec buffer, void* c, stream_notify_t notify, std::uint32_t flag = 0);
            expected<AsyncResult> readfrom_async(view::wvec buffer, NetAddrPort& addr, void* c, recvfrom_notify_t notify, std::uint32_t flag = 0);
            expected<AsyncResult> writeto_async(view::rvec buffer, const NetAddrPort& addr, void* c, stream_notify_t notify, std::uint32_t flag = 0);

            // async I/O methods with lambda
            // these are wrappers of above

            template <class Fn, class BufMgr, class Del>
            expected<AsyncResult> read_async(AsyncContData<Fn, BufMgr, Del>* data_ptr, bool no_del_if_error = false, std::uint32_t flag = 0) {
                if (!data_ptr) {
                    return unexpect("data_ptr must not be null", error::Category::lib, error::fnet_usage_error);
                }

                using HeapData = AsyncContData<Fn, BufMgr, Del>;

                auto lambda = [](Socket&& socket, void* c, NotifyResult&& result) {
                    HeapData* data_ptr = static_cast<HeapData*>(c);
                    const auto d = helper::defer([&] {
                        data_ptr->del(data_ptr);
                    });
                    (data_ptr->fn)(std::move(socket), data_ptr->buffer_mgr, std::move(result));
                };

                return read_async(data_ptr->buffer_mgr.get_buffer(), data_ptr, +lambda, flag)
                    .and_then([&](AsyncResult&& r) -> expected<AsyncResult> {
                        if (r.state == NotifyState::done) {
                            lambda(std::move(*this), data_ptr, r.processed_bytes);
                        }
                        return std::move(r);
                    })
                    .or_else([&](error::Error&& err) -> expected<AsyncResult> {
                        const auto d = helper::defer([&] {
                            if (!no_del_if_error) {
                                data_ptr->del(data_ptr);
                            }
                        });
                        return unexpect(std::move(err));
                    });
            }

            template <class Notify, class Fn, class BufMgr, class Del>
            expected<AsyncResult> read_async_deferred(AsyncContDataWithNotify<Notify, AsyncContData<Fn, BufMgr, Del>>* data_ptr, bool no_del_if_error = false, std::uint32_t flag = 0) {
                if (!data_ptr) {
                    return unexpect("data_ptr must not be null", error::Category::lib, error::fnet_usage_error);
                }

                using HeapData = AsyncContDataWithNotify<Notify, AsyncContData<Fn, BufMgr, Del>>;

                auto lambda = [](Socket&& socket, void* ptr, NotifyResult&& result) {
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
                    data_ptr->notify(DeferredCallback(data_ptr, deferred));
                };

                return read_async(data_ptr->buffer_mgr.get_buffer(), data_ptr, +lambda, flag)
                    .and_then([&](AsyncResult&& r) -> expected<AsyncResult> {
                        if (r.state == NotifyState::done) {
                            lambda(std::move(*this), data_ptr, r.processed_bytes);
                        }
                        return std::move(r);
                    })
                    .or_else([&](error::Error&& err) -> expected<AsyncResult> {
                        const auto d = helper::defer([&] {
                            if (!no_del_if_error) {
                                data_ptr->del(data_ptr);
                            }
                        });
                        return unexpect(std::move(err));
                    });
            }

            template <class Fn, class BufMgr, class Del>
            expected<AsyncResult> write_async(AsyncContData<Fn, BufMgr, Del>* data_ptr, bool no_del_if_error = false, std::uint32_t flag = 0) {
                if (!data_ptr) {
                    return unexpect("data_ptr must not be null", error::Category::lib, error::fnet_usage_error);
                }

                using HeapData = AsyncContData<Fn, BufMgr, Del>;

                auto lambda = [](Socket&& socket, void* c, NotifyResult&& result) {
                    HeapData* data_ptr = static_cast<HeapData*>(c);
                    const auto d = helper::defer([&] {
                        data_ptr->del(data_ptr);
                    });
                    (data_ptr->fn)(std::move(socket), data_ptr->buffer_mgr, std::move(result));
                };

                return write_async(data_ptr->buffer_mgr.get_buffer(), data_ptr, +lambda, flag)
                    .and_then([&](AsyncResult&& r) -> expected<AsyncResult> {
                        if (r.state == NotifyState::done) {
                            lambda(std::move(*this), data_ptr, r.processed_bytes);
                        }
                        return std::move(r);
                    })
                    .or_else([&](error::Error&& err) -> expected<AsyncResult> {
                        const auto d = helper::defer([&] {
                            if (!no_del_if_error) {
                                data_ptr->del(data_ptr);
                            }
                        });
                        return unexpect(std::move(err));
                    });
            }

            template <class Notify, class Fn, class BufMgr, class Del>
            expected<AsyncResult> write_async_deferred(AsyncContDataWithNotify<Notify, AsyncContData<Fn, BufMgr, Del>>* data_ptr, bool no_del_if_error = false, std::uint32_t flag = 0) {
                if (!data_ptr) {
                    return unexpect("data_ptr must not be null", error::Category::lib, error::fnet_usage_error);
                }

                using HeapData = AsyncContDataWithNotify<Notify, AsyncContData<Fn, BufMgr, Del>>;

                auto lambda = [](Socket&& socket, void* c, NotifyResult&& result) {
                    HeapData* data_ptr = static_cast<HeapData*>(c);
                    const auto deferred = [&](void* ptr, bool del_only) {
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
                    data_ptr->notify(DeferredCallback(data_ptr, deferred));
                };

                return write_async(data_ptr->buffer_mgr.get_buffer(), data_ptr, +lambda, flag)
                    .and_then([&](AsyncResult&& r) -> expected<AsyncResult> {
                        if (r.state == NotifyState::done) {
                            lambda(std::move(*this), data_ptr, r.processed_bytes);
                        }
                        return std::move(r);
                    })
                    .or_else([&](error::Error&& err) -> expected<AsyncResult> {
                        const auto d = helper::defer([&] {
                            if (!no_del_if_error) {
                                data_ptr->del(data_ptr);
                            }
                        });

                        return unexpect(std::move(err));
                    });
            }

            template <class Fn, class BufMgr, class Del>
            expected<AsyncResult> readfrom_async(AsyncContData<Fn, BufMgr, Del>* data_ptr, bool no_del_if_error = false, std::uint32_t flag = 0) {
                if (!data_ptr) {
                    return unexpect("data_ptr must not be null", error::Category::lib, error::fnet_usage_error);
                }

                using HeapData = AsyncContData<Fn, BufMgr, Del>;

                auto lambda = [](Socket&& socket, NetAddrPort&& addr, void* c, NotifyResult&& result) {
                    HeapData* data_ptr = static_cast<HeapData*>(c);
                    const auto d = helper::defer([&] {
                        data_ptr->del(data_ptr);
                    });

                    (data_ptr->fn)(std::move(socket), data_ptr->buffer_mgr, std::move(addr), std::move(result));
                };

                return readfrom_async(data_ptr->buffer_mgr.get_buffer(), data_ptr->buffer_mgr.get_address(), data_ptr, +lambda, flag)
                    .and_then([&](AsyncResult&& r) -> expected<AsyncResult> {
                        if (r.state == NotifyState::done) {
                            lambda(std::move(*this), std::move(data_ptr->buffer_mgr.get_address()), data_ptr, r.processed_bytes);
                        }
                        return std::move(r);
                    })
                    .or_else([&](error::Error&& err) -> expected<AsyncResult> {
                        const auto d = helper::defer([&] {
                            if (!no_del_if_error) {
                                data_ptr->del(data_ptr);
                            }
                        });
                        return unexpect(std::move(err));
                    });
            }

            template <class Notify, class Fn, class BufMgr, class Del>
            expected<AsyncResult> readfrom_async_deferred(AsyncContDataWithAddress<AsyncContDataWithNotify<Notify, AsyncContData<Fn, BufMgr, Del>>>* data_ptr, bool no_del_if_error = false, std::uint32_t flag = 0) {
                if (!data_ptr) {
                    return unexpect("data_ptr must not be null", error::Category::lib, error::fnet_usage_error);
                }

                using HeapData = AsyncContDataWithAddress<AsyncContDataWithNotify<Notify, AsyncContData<Fn, BufMgr, Del>>>;

                auto lambda = [](Socket&& socket, NetAddrPort&& addr, void* c, NotifyResult&& result) {
                    HeapData* data_ptr = static_cast<HeapData*>(c);
                    const auto deferred = [&](void* ptr, bool del_only) {
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
                    data_ptr->notify(DeferredCallback(data_ptr, deferred));
                };

                return readfrom_async(data_ptr->buffer_mgr.get_buffer(), data_ptr->buffer_mgr.get_address(), data_ptr, +lambda, flag)
                    .and_then([&](AsyncResult&& r) -> expected<AsyncResult> {
                        if (r.state == NotifyState::done) {
                            lambda(std::move(*this), std::move(data_ptr->buffer_mgr.get_address()), data_ptr, r.processed_bytes);
                        }
                        return std::move(r);
                    })
                    .or_else([&](error::Error&& err) -> expected<AsyncResult> {
                        const auto d = helper::defer([&] {
                            if (!no_del_if_error) {
                                data_ptr->del(data_ptr);
                            }
                        });
                        return unexpect(std::move(err));
                    });
            }

            template <class Fn, class BufMgr, class Del>
            expected<AsyncResult> writeto_async(AsyncContData<Fn, BufMgr, Del>* data_ptr, bool no_del_if_error = false, std::uint32_t flag = 0) {
                if (!data_ptr) {
                    return unexpect("data_ptr must not be null", error::Category::lib, error::fnet_usage_error);
                }

                using HeapData = AsyncContData<Fn, BufMgr, Del>;

                auto lambda = [](Socket&& socket, void* c, NotifyResult&& result) {
                    HeapData* data_ptr = static_cast<HeapData*>(c);

                    const auto d = helper::defer([&] {
                        data_ptr->del(data_ptr);
                    });

                    (data_ptr->fn)(std::move(socket), data_ptr->buffer_mgr, std::move(result));
                };

                return writeto_async(data_ptr->buffer_mgr.get_buffer(), data_ptr->buffer_mgr.get_address(), data_ptr, +lambda, flag)
                    .and_then([&](AsyncResult&& r) -> expected<AsyncResult> {
                        if (r.state == NotifyState::done) {
                            lambda(std::move(*this), data_ptr, r.processed_bytes);
                        }
                        return std::move(r);
                    })
                    .or_else([&](error::Error&& err) -> expected<AsyncResult> {
                        const auto d = helper::defer([&] {
                            if (!no_del_if_error) {
                                data_ptr->del(data_ptr);
                            }
                        });
                        return unexpect(std::move(err));
                    });
            }

            template <class Notify, class Fn, class BufMgr, class Del>
            expected<AsyncResult> writeto_async_deferred(AsyncContDataWithNotify<Notify, AsyncContData<Fn, BufMgr, Del>>* data_ptr, bool no_del_if_error = false, std::uint32_t flag = 0) {
                if (!data_ptr) {
                    return unexpect("data_ptr must not be null", error::Category::lib, error::fnet_usage_error);
                }

                using HeapData = AsyncContDataWithNotify<Notify, AsyncContData<Fn, BufMgr, Del>>;

                auto lambda = [](Socket&& socket, void* c, NotifyResult&& result) {
                    HeapData* data_ptr = static_cast<HeapData*>(c);
                    const auto deferred = [&](void* ptr, bool del_only) {
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
                    data_ptr->notify(DeferredCallback(data_ptr, deferred));
                };

                return writeto_async(data_ptr->buffer_mgr.get_buffer(), data_ptr->buffer_mgr.get_address(), data_ptr, +lambda, flag)
                    .and_then([&](AsyncResult&& r) -> expected<AsyncResult> {
                        if (r.state == NotifyState::done) {
                            lambda(std::move(*this), data_ptr, r.processed_bytes);
                        }
                        return std::move(r);
                    })
                    .or_else([&](error::Error&& err) -> expected<AsyncResult> {
                        const auto d = helper::defer([&] {
                            if (!no_del_if_error) {
                                data_ptr->del(data_ptr);
                            }
                        });

                        return unexpect(std::move(err));
                    });
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
