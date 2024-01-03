/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
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

    expected<void> Socket::set_skipnotify(bool skip_notif, bool skip_event) {
        return unexpect(error::Error("not supported on linux", error::Category::lib, error::fnet_usage_error));
    }

    Canceler::~Canceler() {
        // TODO(on-keyday): implement
    }

    expected<void> Canceler::cancel() {
        return unexpect("operation not supported", error::Category::lib, error::fnet_async_error);
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
                    if (tbl->r.cb.call) {
                        auto cb = tbl->r.cb;
                        tbl->r.cb.call(&cb, &tbl->r, std::nullopt);
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
                    if (tbl->w.cb.call) {
                        auto cb = tbl->w.cb;
                        tbl->w.cb.call(&cb, &tbl->w, std::nullopt);
                    }
                }
            }
        }
    }  // namespace event

    void call_stream(futils::fnet::NotifyCallback* cb, IOTableHeader* base, NotifyResult&& result) {
        auto hdr = static_cast<EpollIOTableHeader*>(base);
        auto sock = make_socket(hdr->base);
        hdr->cb = {};     // clear for epoll_lock
        hdr->l.unlock();  // release, so can do next IO
        auto notify = reinterpret_cast<stream_notify_t>(cb->notify);
        notify(std::move(sock), cb->user, std::move(result));
    }

    void call_recvfrom(futils::fnet::NotifyCallback* cb, IOTableHeader* base, NotifyResult&& result) {
        auto hdr = static_cast<EpollIOTableHeader*>(base);
        auto sock = make_socket(hdr->base);
        hdr->cb = {};     // clear for epoll_lock
        hdr->l.unlock();  // release, so can do next IO
        auto notify = reinterpret_cast<recvfrom_notify_t>(cb->notify);
        notify(std::move(sock), NetAddrPort(), cb->user, std::move(result));
    }

    AsyncResult on_success(EpollTable* t, std::uint64_t code, bool w, size_t bytes) {
        // skip callback call, so should decrement reference count and release lock here
        auto dec = t->decr();
        assert(dec != 0);
        if (w) {
            t->w.l.unlock();
            t->w.cb = {};  // clear for epoll lock
        }
        else {
            t->r.l.unlock();
            t->r.cb = {};  // clear for epoll lock
        }
        return {Canceler(), NotifyState::done, bytes};
    }

    expected<AsyncResult> handle_result(ssize_t result, EpollTable* t, EpollIOTableHeader& io, std::uint64_t cancel_code, bool is_write) {
        if (result != -1) {
            return on_success(t, cancel_code, is_write, result);
        }
        if (get_error() != EWOULDBLOCK &&
            get_error() != EAGAIN) {
            auto dec = t->decr();
            assert(dec != 0);
            const auto d = helper::defer([&] { io.l.unlock(); });
            return unexpect(error::Errno());
        }
        // TODO(on-keyday): implement cancel
        return AsyncResult{Canceler(), NotifyState::wait, 0};
    }

    expected<AsyncResult> async_stream_operation(EpollTable* t, view::rvec buffer, std::uint32_t flag, void* c, stream_notify_t notify, bool is_write) {
        EpollIOTableHeader& io = is_write ? static_cast<EpollIOTableHeader&>(t->w) : t->r;
        auto& cb = io.cb;

        return io.l.try_lock()  // lock released by call_stream
            .and_then([&](std::uint64_t cancel_code) {
                io.base = t;
                io.epoll_lock = true;
                const auto d = helper::defer([&] { io.epoll_lock = false; });  // anyway, release finally
                cb.set_notify(c, notify, call_stream);
                t->incr();  // increment, released by Socket passed to notify() or on_success()
                auto result = is_write ? lazy::send_(t->sock, buffer.as_char(), buffer.size(), flag)
                                       : lazy::recv_(t->sock, (char*)buffer.as_char(), buffer.size(), flag);

                return handle_result(result, t, io, cancel_code, is_write);
            });
    }

    expected<AsyncResult> async_packet_operation(EpollTable* t, view::rvec buffer, std::uint32_t flag, void* c, auto notify,
                                                 auto&& get_addr,
                                                 bool is_write) {
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
                if (is_write) {
                    cb.set_notify(c, notify, call_stream);
                }
                else {
                    cb.set_notify(c, notify, call_recvfrom);  // special handling
                }
                t->incr();  // increment, released by Socket passed to notify() or on_success()

                auto result = is_write
                                  ? lazy::sendto_(t->sock, buffer.as_char(), buffer.size(), flag, addr, *len)
                                  : lazy::recvfrom_(t->sock, (char*)buffer.as_char(), buffer.size(), flag, addr, len);

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
            return async_stream_operation(t, buffer, flag, c, notify, false);
        });
    }

    expected<AsyncResult> Socket::write_async(view::rvec buffer, void* c, stream_notify_t notify, std::uint32_t flag) {
        return get_tbl(ctx).and_then([&](EpollTable* t) {
            return async_stream_operation(t, buffer, flag, c, notify, true);
        });
    }

    expected<AsyncResult> Socket::readfrom_async(view::wvec buffer, NetAddrPort& addr, void* c, recvfrom_notify_t notify, std::uint32_t flag) {
        return get_tbl(ctx).and_then([&](EpollTable* t) -> expected<AsyncResult> {
            sockaddr_storage storage;
            socklen_t size = sizeof(storage);
            return async_packet_operation(
                       t, buffer, flag, c, notify, [&](EpollIOTableHeader& hdr) -> std::pair<sockaddr*, socklen_t*> {
                           return std::make_pair((sockaddr*)&storage, &size);
                       },
                       false)
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
                t, buffer, flag, c, notify, [&](EpollIOTableHeader& hdr) -> std::pair<sockaddr*, socklen_t*> {
                    auto [ptr, len] = NetAddrPort_to_sockaddr(&storage, addr);
                    size = len;
                    return std::make_pair(ptr, &size);
                },
                true);
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
