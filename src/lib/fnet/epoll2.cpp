/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/dll/dllcpp.h>
#include <fnet/sock_internal.h>
#include <fnet/dll/lazy/sockdll.h>
#include <fnet/dll/errno.h>
#include <fnet/event/io.h>
#include <fnet/socket.h>
#include <cstdint>

namespace futils::fnet {

    Canceler make_canceler(std::uint64_t code, bool w, void* b) {
        Canceler c;
        c.cancel_code = code;
        c.w = w;
        c.b = b;
        static_cast<SockTable*>(b)->incr();  // b must be SockTable
        return c;
    }

    expected<void> Socket::set_skipnotify(bool skip_notif, bool skip_event) {
        return unexpect(error::Error("not supported on linux", error::Category::lib, error::fnet_usage_error));
    }

    Canceler::~Canceler() {
        if (b) {
            static_cast<EpollTable*>(b)->decr();
        }
    }

    expected<void> Canceler::cancel() {
        if (!b) {
            return unexpect("operation completed", error::Category::lib, error::fnet_async_error);
        }
        auto t = static_cast<EpollTable*>(b);
        // at here, if we can get callback, we execute it
        EpollIOTableHeader& io = w ? static_cast<EpollIOTableHeader&>(t->w) : t->r;
        void (*notify)() = nullptr;
        void* user = nullptr;
        NotifyCallback::call_t call = nullptr;
        auto result = io.l.interrupt(cancel_code, [&]() -> expected<void> {
            while (io.epoll_lock) {
                timespec spec;
                spec.tv_sec = 0;
                spec.tv_nsec = 1;
                nanosleep(&spec, &spec);
            }
            if (auto call_ = io.cb.call.exchange(nullptr)) {
                call = call_;
                notify = io.cb.notify;
                user = io.cb.user;
                return expected<void>();
            }
            return unexpect("operation already done", error::Category::lib, error::fnet_async_error);
        });
        if (!result) {
            return unexpect(result.error());
        }
        call(notify, user, &io, unexpect("operation canceled", error::Category::lib, error::fnet_async_error));
        return {};
    }

    namespace event {
        fnet_dll_implement(void) fnet_handle_completion(void* p, void*) {
            auto event = static_cast<epoll_event*>(p);
            auto tbl = static_cast<EpollTable*>(event->data.ptr);
            auto events = event->events;
            if (events & (EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                if (tbl->r.cb.call) {
                    while (tbl->r.epoll_lock) {
                        timespec spec;
                        spec.tv_sec = 0;
                        spec.tv_nsec = 1;
                        nanosleep(&spec, &spec);
                    }
                    if (auto call = tbl->r.cb.call.exchange(nullptr)) {
                        auto notify = tbl->r.cb.notify;
                        auto user = tbl->r.cb.user;
                        call(notify, user, &tbl->r, std::nullopt);
                    }
                }
            }
            if (events & (EPOLLOUT | EPOLLERR | EPOLLHUP)) {
                if (tbl->w.cb.call) {
                    while (tbl->w.epoll_lock) {
                        timespec spec;
                        spec.tv_sec = 0;
                        spec.tv_nsec = 1;
                        nanosleep(&spec, &spec);
                    }
                    if (auto call = tbl->w.cb.call.exchange(nullptr)) {
                        auto notify = tbl->w.cb.notify;
                        auto user = tbl->w.cb.user;
                        call(notify, user, &tbl->w, std::nullopt);
                    }
                }
            }
        }
    }  // namespace event

    void call_stream(void (*notify_base)(), void* user, IOTableHeader* base, NotifyResult&& result) {
        auto hdr = static_cast<EpollIOTableHeader*>(base);
        auto sock = make_socket(hdr->base);
        hdr->l.unlock();  // release, so can do next IO
        auto notify = reinterpret_cast<stream_notify_t>(notify_base);
        notify(std::move(sock), user, std::move(result));
    }

    void call_recvfrom(void (*notify_base)(), void* user, IOTableHeader* base, NotifyResult&& result) {
        auto hdr = static_cast<EpollIOTableHeader*>(base);
        auto sock = make_socket(hdr->base);
        hdr->l.unlock();  // release, so can do next IO
        auto notify = reinterpret_cast<recvfrom_notify_t>(notify_base);
        notify(std::move(sock), NetAddrPort(), user, std::move(result));
    }

    void call_accept(void (*notify_base)(), void* user, IOTableHeader* base, NotifyResult&& result) {
        auto hdr = static_cast<EpollIOTableHeader*>(base);
        auto sock = make_socket(hdr->base);
        hdr->l.unlock();  // release, so can do next IO
        auto notify = reinterpret_cast<accept_notify_t>(notify_base);
        auto accept_ok = sock.accept();
        if (!accept_ok) {
            notify(std::move(sock), Socket(), NetAddrPort(), user, unexpect(accept_ok.error()));
        }
        else {
            auto [accepted, addr] = std::move(*accept_ok);
            notify(std::move(sock), std::move(accepted), std::move(addr), user, std::move(result));
        }
    }

    void call_connect(void (*notify_base)(), void* user, IOTableHeader* base, NotifyResult&& result) {
        auto hdr = static_cast<EpollIOTableHeader*>(base);
        auto sock = make_socket(hdr->base);
        hdr->l.unlock();  // release, so can do next IO
        socklen_t err = 0;
        auto res = sock.get_option(SOL_SOCKET, SO_ERROR, err);
        auto notify = reinterpret_cast<stream_notify_t>(notify_base);
        if (!res) {
            notify(std::move(sock), user, unexpect(res.error()));
        }
        else {
            if (err != 0) {
                notify(std::move(sock), user, unexpect(error::Error(err, error::Category::os)));
            }
            else {
                notify(std::move(sock), user, std::move(result));
            }
        }
    }

    AsyncResult on_success(EpollTable* t, std::uint64_t code, bool w, size_t bytes) {
        // skip callback call, so should decrement reference count and release lock here
        auto dec = t->decr();
        assert(dec != 0);
        if (w) {
            t->w.cb.call = nullptr;
            t->w.cb.notify = nullptr;
            t->w.cb.user = nullptr;
            t->w.l.unlock();
        }
        else {
            t->r.cb.call = nullptr;
            t->r.cb.notify = nullptr;
            t->r.cb.user = nullptr;
            t->r.l.unlock();
        }
        return {Canceler(), NotifyState::done, bytes};
    }

    expected<AsyncResult> handle_result(ssize_t result, EpollTable* t, EpollIOTableHeader& io, std::uint64_t cancel_code, bool is_write) {
        if (result != -1) {
            return on_success(t, cancel_code, is_write, result);
        }
        if (get_error() != EWOULDBLOCK &&
            get_error() != EAGAIN &&
            get_error() != EINPROGRESS) {
            auto dec = t->decr();
            assert(dec != 0);
            const auto d = helper::defer([&] { io.l.unlock(); });
            return unexpect(error::Errno());
        }
        // TODO(on-keyday): implement cancel
        return AsyncResult{make_canceler(cancel_code, is_write, t), NotifyState::wait, 0};
    }

    expected<AsyncResult> async_stream_operation(EpollTable* t, view::rvec buffer, std::uint32_t flag, void* c,
                                                 stream_notify_t notify, bool is_write, NotifyCallback::call_t call, auto&& do_io) {
        EpollIOTableHeader& io = is_write ? static_cast<EpollIOTableHeader&>(t->w) : t->r;
        auto& cb = io.cb;

        return io.l.try_lock()  // lock released by call_stream
            .and_then([&](std::uint64_t cancel_code) {
                io.base = t;
                io.epoll_lock = true;
                const auto d = helper::defer([&] { io.epoll_lock = false; });  // anyway, release finally
                cb.set_notify(c, notify, call);
                t->incr();  // increment, released by Socket passed to notify() or on_success()
                auto result = do_io(t->sock, buffer, flag);

                return handle_result(result, t, io, cancel_code, is_write);
            });
    }

    expected<AsyncResult> async_packet_operation(EpollTable* t, view::rvec buffer, std::uint32_t flag, void* c, auto notify,
                                                 auto&& get_addr,
                                                 bool is_write, NotifyCallback::call_t call, auto&& do_io) {
        EpollIOTableHeader& io = is_write ? static_cast<EpollIOTableHeader&>(t->w) : t->r;
        auto& cb = io.cb;

        return io.l.try_lock()  // lock released by call_stream
            .and_then([&](std::uint64_t cancel_code) -> expected<AsyncResult> {
                io.base = t;
                auto [addr, len] = get_addr(io);
                if (!addr) {
                    io.l.unlock();
                    return unexpect(error::Error("unsupported address type", error::Category::lib, error::fnet_usage_error));
                }
                io.epoll_lock = true;
                const auto d = helper::defer([&] { io.epoll_lock = false; });  // anyway, release finally
                cb.set_notify(c, notify, call);
                t->incr();  // increment, released by Socket passed to notify() or on_success()

                auto result = do_io(t->sock, buffer, flag, addr, len);

                return handle_result(result, t, io, cancel_code, is_write);
            });
    }

    expected<EpollTable*> get_tbl(void* a) {
        if (!a) {
            return unexpect("socket not initialized", error::Category::lib, error::fnet_usage_error);
        }
        return static_cast<EpollTable*>(a);
    }

    expected<AsyncResult> Socket::read_async(view::wvec buffer, void* c, stream_notify_t notify, std::uint32_t flag) {
        return get_tbl(ctx).and_then([&](EpollTable* t) {
            return async_stream_operation(t, buffer, flag, c, notify, false, call_stream, [](auto sock, auto buffer, auto flag) {
                return lazy::recv_(sock, (char*)buffer.as_char(), buffer.size(), flag);
            });
        });
    }

    expected<AsyncResult> Socket::write_async(view::rvec buffer, void* c, stream_notify_t notify, std::uint32_t flag) {
        return get_tbl(ctx).and_then([&](EpollTable* t) {
            return async_stream_operation(t, buffer, flag, c, notify, true, call_stream, [](auto sock, auto buffer, auto flag) {
                return lazy::send_(sock, (const char*)buffer.as_char(), buffer.size(), flag);
            });
        });
    }

    expected<AsyncResult> Socket::readfrom_async(view::wvec buffer, NetAddrPort& addr, void* c, recvfrom_notify_t notify, std::uint32_t flag) {
        return get_tbl(ctx).and_then([&](EpollTable* t) -> expected<AsyncResult> {
            sockaddr_storage storage;
            socklen_t size = sizeof(storage);
            return async_packet_operation(
                       t, buffer, flag, c, notify,
                       [&](EpollIOTableHeader& hdr) -> std::pair<sockaddr*, socklen_t*> { return std::make_pair((sockaddr*)&storage, &size); },
                       false, call_recvfrom, [](auto sock, auto buffer, auto flag, auto addr, auto len) { return lazy::recvfrom_(sock, (char*)buffer.as_char(), buffer.size(), flag, addr, len); })
                .and_then([&](AsyncResult&& r) -> expected<AsyncResult> {
                    if (r.state == NotifyState::done) {
                        addr = sockaddr_to_NetAddrPort((sockaddr*)&storage, size);
                    }
                    return std::move(r);
                });
        });
    }

    expected<AsyncResult> Socket::writeto_async(view::rvec buffer, const NetAddrPort& addr, void* c, stream_notify_t notify, std::uint32_t flag) {
        return get_tbl(ctx).and_then([&](EpollTable* t) {
            sockaddr_storage storage;
            socklen_t size = sizeof(storage);
            return async_packet_operation(
                t, buffer, flag, c, notify,
                [&](EpollIOTableHeader& hdr) -> std::pair<sockaddr*, socklen_t*> {
                    auto [ptr, len] = NetAddrPort_to_sockaddr(&storage, addr);
                    size = len;
                    return std::make_pair(ptr, &size);
                },
                true, call_stream,
                [](auto sock, auto buffer, auto flag, auto addr, auto len) {
                    return lazy::sendto_(sock, (const char*)buffer.as_char(), buffer.size(), flag, addr, *len);
                });
        });
    }

    expected<AcceptAsyncResult<Socket>> Socket::accept_async(NetAddrPort& addr, void* c, accept_notify_t notify, std::uint32_t flag) {
        return get_tbl(ctx).and_then([&](EpollTable* t) -> expected<AcceptAsyncResult<Socket>> {
            sockaddr_storage storage;
            auto r = async_packet_operation(
                t, view::rvec(), flag, c, notify,
                [&](EpollIOTableHeader& hdr) -> std::pair<sockaddr*, socklen_t*> {
                    socklen_t size = sizeof(storage);
                    return std::make_pair((sockaddr*)&storage, &size);
                },
                false, call_accept,
                [](auto sock, auto buffer, auto flag, auto addr, auto len) {
                    return lazy::accept_(sock, addr, len);
                });
            if (!r) {
                return unexpect(r.error());
            }
            AcceptAsyncResult<Socket> result;
            result.cancel = std::move(r->cancel);
            result.state = r->state;
            if (result.state == NotifyState::done) {
                auto sock = r->processed_bytes;
                auto r = setup_socket(sock, t->event);
                if (!r) {
                    return unexpect(r.error());
                }
                result.socket = std::move(*r);
                addr = sockaddr_to_NetAddrPort((sockaddr*)&storage, sizeof(storage));
            }
            return result;
        });
    }

    expected<void> Socket::connect_ex_loopback_optimize(const NetAddrPort& addr) {
        return unexpect(error::Error("not supported on linux", error::Category::lib, error::fnet_usage_error));
    }

    expected<AsyncResult> Socket::connect_async(const NetAddrPort& addr, void* c, stream_notify_t notify, std::uint32_t flag) {
        return get_tbl(ctx).and_then([&](EpollTable* t) {
            socklen_t tmp = 0;
            sockaddr_storage storage;
            return async_packet_operation(
                t, view::rvec(), flag, c, notify,
                [&](EpollIOTableHeader& hdr) -> std::pair<sockaddr*, socklen_t*> {
                    auto [ptr, len] = NetAddrPort_to_sockaddr(&storage, addr);
                    tmp = len;
                    return std::make_pair(ptr, &tmp);
                },
                true, call_connect,
                [](auto sock, auto buffer, auto flag, auto addr, auto len) {
                    return lazy::connect_(sock, addr, *len);
                });
        });
    }

    SockTable* make_sock_table(std::uintptr_t sock, event::IOEvent* event) {
        auto tbl = new_glheap(EpollTable);
        tbl->sock = sock;
        tbl->event = event;
        return tbl;
    }

    void destroy_platform(SockTable* s) {
        delete_glheap(static_cast<EpollTable*>(s));
    }

    void sockclose(std::uintptr_t sock) {
        lazy::close_(sock);
    }

    void set_nonblock(std::uintptr_t sock, bool yes) {
        u_long nb = yes ? 1 : 0;
        lazy::ioctl_(sock, FIONBIO, &nb);
    }

}  // namespace futils::fnet
