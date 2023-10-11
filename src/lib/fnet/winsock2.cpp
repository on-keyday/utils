/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/dll/dllcpp.h>
#include <fnet/sock_internal.h>
#include <fnet/dll/lazy/sockdll.h>
#include <fnet/dll/errno.h>
#include <fnet/socket.h>

namespace utils::fnet {

    Canceler make_canceler(std::uint64_t code, bool w, void* b) {
        Canceler c;
        c.cancel_code = code;
        c.w = w;
        c.b = b;
        static_cast<SockTable*>(b)->incr();  // b must be SockTable
        return c;
    }

    Canceler::~Canceler() {
        if (b) {
            static_cast<WinSockTable*>(b)->decr();
        }
    }

    expected<void> Canceler::cancel() {
        if (!b) {
            return unexpect("operation completed", error::Category::lib, error::fnet_async_error);
        }
        auto t = static_cast<WinSockTable*>(b);
        WinSockIOTableHeader& io = w ? static_cast<WinSockIOTableHeader&>(t->w) : t->r;
        return io.l.interrupt(cancel_code, [&]() -> expected<void> {
            if (!lazy::CancelIoEx_(HANDLE(io.base->sock), &io.ol)) {
                if (get_error() == ERROR_NOT_FOUND) {
                    return unexpect("operation not found", error::Category::os);
                }
                else {
                    return unexpect(error::Errno());
                }
            }
            return {};
        });
    }
    namespace event {
        fnet_dll_implement(void) fnet_handle_completion(void* ptr, void*) {
            auto entry = (OVERLAPPED_ENTRY*)ptr;
            auto hdr = (WinSockIOTableHeader*)entry->lpOverlapped;
            error::Error err;
            DWORD nt = 0;
            if (!GetOverlappedResult((HANDLE)hdr->base->sock, entry->lpOverlapped, &nt, false)) {
                err = error::Errno();
            }
            // cb.call must release hdr
            auto cb = hdr->cb;
            if (err) {
                cb.call(&cb, hdr, unexpect(std::move(err)));
            }
            else {
                cb.call(&cb, hdr, entry->dwNumberOfBytesTransferred);
            }
        }
    }  // namespace event

    void call_stream(utils::fnet::NotifyCallback* cb, void* base, NotifyResult&& result) {
        auto hdr = (WinSockIOTableHeader*)base;
        auto sock = make_socket(hdr->base);
        hdr->l.unlock();  // release, so can do next IO
        auto notify = reinterpret_cast<stream_notify_t>(cb->notify);
        notify(std::move(sock), cb->user, std::move(result));
    }

    void call_recvfrom(utils::fnet::NotifyCallback* cb, void* base, NotifyResult&& result) {
        auto hdr = (WinSockReadTable*)base;
        auto sock = make_socket(hdr->base);
        auto addr = sockaddr_to_NetAddrPort((sockaddr*)&hdr->from, hdr->from_len);
        hdr->l.unlock();  // release, so can do next IO
        auto notify = reinterpret_cast<recvfrom_notify_t>(cb->notify);
        notify(std::move(sock), std::move(addr), cb->user, std::move(result));
    }

    AsyncResult on_success(WinSockTable* t, std::uint64_t code, bool w, size_t bytes) {
        if (t->skip_notif) {
            // skip callback call, so should decrement reference count and release lock here
            t->decr();
            if (w) {
                t->w.l.unlock();
            }
            else {
                t->r.l.unlock();
            }
            return {Canceler(), NotifyState::done, bytes};
        }
        return {make_canceler(code, w, t), NotifyState::wait, 0};
    }

    expected<AsyncResult> handle_result(int result, WinSockTable* t, WinSockIOTableHeader& io, std::uint64_t cancel_code, size_t proc_bytes, bool is_write) {
        if (result == 0) {
            return on_success(t, cancel_code, is_write, proc_bytes);
        }
        if (get_error() != WSA_IO_PENDING) {
            t->decr();
            const auto d = helper::defer([&] { io.l.unlock(); });
            return unexpect(error::Errno());
        }
        return AsyncResult{make_canceler(cancel_code, is_write, t), NotifyState::wait, 0};
    }

    expected<AsyncResult> async_stream_operation(WinSockTable* t, view::rvec buffer, std::uint32_t flag, void* c, stream_notify_t notify, bool is_write) {
        WinSockIOTableHeader& io = is_write ? static_cast<WinSockIOTableHeader&>(t->w) : t->r;
        auto& cb = io.cb;

        return io.l.try_lock()  // lock released by call_stream
            .and_then([&](std::uint64_t cancel_code) {
                io.base = t;
                io.set_buffer(buffer);
                cb.set_notify(c, notify, call_stream);
                DWORD iflags = flag;
                DWORD proc_bytes = 0;
                t->incr();  // increment, released by Socket passed to notify() or on_success()

                auto result = is_write
                                  ? lazy::WSASend_(t->sock, io.bufs, 1, &proc_bytes, iflags, &io.ol, nullptr)
                                  : lazy::WSARecv_(t->sock, io.bufs, 1, &proc_bytes, &iflags, &io.ol, nullptr);

                return handle_result(result, t, io, cancel_code, proc_bytes, is_write);
            });
    }

    expected<AsyncResult> async_packet_operation(WinSockTable* t, view::rvec buffer, std::uint32_t flag, void* c, auto notify,
                                                 auto&& get_addr,
                                                 bool is_write) {
        WinSockIOTableHeader& io = is_write ? static_cast<WinSockIOTableHeader&>(t->w) : t->r;
        auto& cb = io.cb;

        return io.l.try_lock()  // lock released by call_stream
            .and_then([&](std::uint64_t cancel_code) -> expected<AsyncResult> {
                io.base = t;
                auto [addr, len] = get_addr(io);
                if (!addr) {
                    io.l.unlock();
                    return unexpect(error::Error("unsupported address type", error::Category::lib, error::fnet_usage_error));
                }
                io.set_buffer(buffer);
                if (is_write) {
                    cb.set_notify(c, notify, call_stream);
                }
                else {
                    cb.set_notify(c, notify, call_recvfrom);  // special handling
                }
                DWORD iflags = flag;
                DWORD proc_bytes = 0;
                t->incr();  // increment, released by Socket passed to notify() or on_success()

                auto result = is_write
                                  ? lazy::WSASendTo_(t->sock, io.bufs, 1, &proc_bytes, iflags, addr, *len, &io.ol, nullptr)
                                  : lazy::WSARecvFrom_(t->sock, io.bufs, 1, &proc_bytes, &iflags, addr, len, &io.ol, nullptr);

                return handle_result(result, t, io, cancel_code, proc_bytes, is_write);
            });
    }

    expected<WinSockTable*> get_tbl(void* a) {
        if (!a) {
            return unexpect("socket not initialized", error::Category::lib, error::fnet_usage_error);
        }
        return static_cast<WinSockTable*>(a);
    }

    expected<AsyncResult> Socket::read_async(view::wvec buffer, void* c, stream_notify_t notify, std::uint32_t flag) {
        return get_tbl(ctx).and_then([&](WinSockTable* t) {
            return async_stream_operation(t, buffer, flag, c, notify, false);
        });
    }

    expected<AsyncResult> Socket::write_async(view::rvec buffer, void* c, stream_notify_t notify, std::uint32_t flag) {
        return get_tbl(ctx).and_then([&](WinSockTable* t) {
            return async_stream_operation(t, buffer, flag, c, notify, true);
        });
    }

    expected<AsyncResult> Socket::readfrom_async(view::wvec buffer, NetAddrPort& addr, void* c, recvfrom_notify_t notify, std::uint32_t flag) {
        return get_tbl(ctx).and_then([&](WinSockTable* t) {
            return async_packet_operation(
                       t, buffer, flag, c, notify, [&](WinSockIOTableHeader& hdr) -> std::pair<sockaddr*, int*> {
                           auto& tbl = static_cast<WinSockReadTable&>(hdr);
                           tbl.from_len = sizeof(tbl.from);
                           return std::make_pair((sockaddr*)&tbl.from, &tbl.from_len);
                       },
                       false)
                .and_then([&](AsyncResult&& r) -> expected<AsyncResult> {
                    if (r.state == NotifyState::done) {
                        addr = sockaddr_to_NetAddrPort((sockaddr*)&t->r.from, t->r.from_len);
                    }
                    return std::move(r);
                });
        });
    }

    expected<AsyncResult> Socket::writeto_async(view::rvec buffer, const NetAddrPort& addr, void* c, stream_notify_t notify, std::uint32_t flag) {
        return get_tbl(ctx).and_then([&](WinSockTable* t) {
            int tmp = 0;
            return async_packet_operation(
                t, buffer, flag, c, notify, [&](WinSockIOTableHeader& hdr) -> std::pair<sockaddr*, int*> {
                    auto& tbl = static_cast<WinSockWriteTable&>(hdr);
                    auto [ptr, len] = NetAddrPort_to_sockaddr(&tbl.to, addr);
                    tmp = len;
                    return std::make_pair(ptr, &tmp);
                },
                true);
        });
    }

    expected<void> Socket::set_skipnotify(bool skip_notif, bool skip_event) {
        if (!lazy::SetFileCompletionNotificationModes_.find()) {
            return unexpect(error::Error("SetFileCompletionNotificationModes are not supported in this platform", error::Category::os));
        }
        auto tbl = static_cast<WinSockTable*>(ctx);
        if (!tbl) {
            return unexpect("socket not initialized", error::Category::lib, error::fnet_usage_error);
        }
        byte flag = 0;
        if (skip_event) {
            flag |= FILE_SKIP_SET_EVENT_ON_HANDLE;
        }
        if (skip_notif) {
            flag |= FILE_SKIP_COMPLETION_PORT_ON_SUCCESS;
        }
        if (!lazy::SetFileCompletionNotificationModes_((HANDLE)tbl->sock, flag)) {
            return unexpect(error::Errno());
        }
        tbl->skip_notif = skip_notif;
        return {};
    }

    SockTable* make_sock_table(std::uintptr_t sock, event::IOEvent* e) {
        auto tbl = new_glheap(WinSockTable);
        tbl->sock = sock;
        tbl->event = e;
        return tbl;
    }

    void destroy_platform(SockTable* s) {
        delete_glheap(static_cast<WinSockTable*>(s));
    }

    void sockclose(std::uintptr_t sock) {
        lazy::closesocket_(sock);
    }

    void set_nonblock(std::uintptr_t sock, bool yes) {
        u_long nb = yes ? 1 : 0;
        lazy::ioctlsocket_(sock, FIONBIO, &nb);
    }

}  // namespace utils::fnet
