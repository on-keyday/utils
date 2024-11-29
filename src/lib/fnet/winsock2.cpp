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
#include <fnet/socket.h>

namespace futils::fnet {

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
        // if CancelIoEx success, completion notification is notified by IOCP
        // so we don't need to call completion callback here and not release lock
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
            auto user = hdr->cb.user;
            auto notify = hdr->cb.notify;
            auto call = hdr->cb.call.exchange(nullptr);
            if (err) {
                call(notify, user, hdr, unexpect(std::move(err)));
            }
            else {
                call(notify, user, hdr, entry->dwNumberOfBytesTransferred);
            }
        }
    }  // namespace event

    void call_stream(void (*notify_base)(), void* user, IOTableHeader* base, NotifyResult&& result) {
        auto hdr = static_cast<WinSockIOTableHeader*>(base);
        auto sock = make_socket(hdr->base);
        hdr->l.unlock();  // release, so can do next IO
        auto notify = reinterpret_cast<stream_notify_t>(notify_base);
        notify(std::move(sock), user, std::move(result));
    }

    void call_recvfrom(void (*notify_base)(), void* user, IOTableHeader* base, NotifyResult&& result) {
        auto hdr = static_cast<WinSockReadTable*>(base);
        auto sock = make_socket(hdr->base);
        auto addr = sockaddr_to_NetAddrPort((sockaddr*)&hdr->from, hdr->from_len);
        hdr->l.unlock();  // release, so can do next IO
        auto notify = reinterpret_cast<recvfrom_notify_t>(notify_base);
        notify(std::move(sock), std::move(addr), user, std::move(result));
    }

    auto chain_error(NotifyResult& result, error::Error&& err) {
        if (!result.value().has_value()) {
            auto e = std::move(result.value().error());
            result = NotifyResult{unexpect(error::ErrList{std::move(err), std::move(e)})};
        }
        else {
            result = NotifyResult{unexpect(err)};
        }
    }

    void call_connect(void (*notify_base)(), void* user, IOTableHeader* base, NotifyResult&& result) {
        auto hdr = static_cast<WinSockIOTableHeader*>(base);
        auto sock = make_socket(hdr->base);
        auto raw_sock = hdr->base->sock;
        hdr->l.unlock();  // release, so can do next IO
        auto res = sock.set_option(SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, raw_sock);
        if (!res) {
            chain_error(result, std::move(res.error()));
        }
        auto notify = reinterpret_cast<stream_notify_t>(notify_base);
        notify(std::move(sock), user, std::move(result));
    }

    NetAddrPort get_accept_ex_addr(WinSockReadTable* hdr) {
        sockaddr *paddr, *paddr2;
        INT addr_len, addr2_len;
        lazy::GetAcceptExSockaddrs_(hdr->storage, 0, acceptex_least_size_one, acceptex_least_size_one, &paddr, &addr_len, &paddr2, &addr2_len);
        return sockaddr_to_NetAddrPort(paddr, addr_len);
    }

    void call_accept(void (*notify_base)(), void* user, IOTableHeader* base, NotifyResult&& result) {
        auto hdr = static_cast<WinSockReadTable*>(base);
        auto sock = make_socket(hdr->base);
        auto raw_sock = hdr->base->sock;
        auto accepted_sock = hdr->accept_sock;
        auto event = hdr->base->event;
        auto addr = result.value().has_value() ? get_accept_ex_addr(hdr) : NetAddrPort{};
        hdr->l.unlock();  // release, so can do next IO
        auto accept_notify = reinterpret_cast<accept_notify_t>(notify_base);

        if (!result.value()) {
            sockclose(accepted_sock);
            accept_notify(std::move(sock), Socket(), std::move(addr), user, std::move(result));
            return;
        }

        auto s = setup_socket(accepted_sock, event);  // ownership of accepted_sock were transferred to setup_socket
        if (!s) {
            chain_error(result, std::move(s.error()));
            accept_notify(std::move(sock), Socket(), std::move(addr), user, std::move(result));
            return;
        }

        auto res = s->set_option(SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, raw_sock);
        if (!res) {
            chain_error(result, std::move(res.error()));
            accept_notify(std::move(sock), Socket(), std::move(addr), user, std::move(result));
            return;
        }

        accept_notify(std::move(sock), std::move(*s), std::move(addr), user, std::move(result));
    }

    AsyncResult on_success(WinSockTable* t, std::uint64_t code, bool w, size_t bytes) {
        if (t->skip_notif) {
            // skip callback call, so should decrement reference count and release lock here
            auto dec = t->decr();
            assert(dec != 0);
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
            auto dec = t->decr();
            assert(dec != 0);
            const auto d = helper::defer([&] { io.l.unlock(); });
            return unexpect(error::Errno());
        }
        return AsyncResult{make_canceler(cancel_code, is_write, t), NotifyState::wait, 0};
    }

    expected<AsyncResult> async_stream_operation(WinSockTable* t, view::rvec buffer, std::uint32_t flag, void* c, stream_notify_t notify, bool is_write, NotifyCallback::call_t call, auto&& do_io) {
        WinSockIOTableHeader& io = is_write ? static_cast<WinSockIOTableHeader&>(t->w) : t->r;
        auto& cb = io.cb;

        return io.l.try_lock()  // lock released by call_stream
            .and_then([&](std::uint64_t cancel_code) {
                io.base = t;
                io.set_buffer(buffer);
                cb.set_notify(c, notify, call);
                DWORD iflags = flag;
                DWORD proc_bytes = 0;
                t->incr();  // increment, released by Socket passed to notify() or on_success()

                auto result = do_io(t->sock, io.bufs, proc_bytes, iflags, &io.ol);

                return handle_result(result, t, io, cancel_code, proc_bytes, is_write);
            });
    }

    expected<AsyncResult> async_packet_operation(WinSockTable* t, view::rvec buffer, std::uint32_t flag, void* c, auto notify,
                                                 auto&& get_addr,
                                                 bool is_write, NotifyCallback::call_t call, auto&& do_io) {
        WinSockIOTableHeader& io = is_write ? static_cast<WinSockIOTableHeader&>(t->w) : t->r;
        auto& cb = io.cb;

        return io.l.try_lock()  // lock released by call
            .and_then([&](std::uint64_t cancel_code) -> expected<AsyncResult> {
                io.base = t;
                auto [addr, len] = get_addr(io);
                if (!addr) {
                    io.l.unlock();
                    return unexpect(error::Error("unsupported address type", error::Category::lib, error::fnet_usage_error));
                }
                io.set_buffer(buffer);
                cb.set_notify(c, notify, call);
                DWORD iflags = flag;
                DWORD proc_bytes = 0;
                t->incr();  // increment, released by Socket passed to notify() or on_success()

                auto result = do_io(t->sock, io.bufs, proc_bytes, iflags, addr, len, &io.ol);

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
            return async_stream_operation(t, buffer, flag, c, notify, false, call_stream, [](auto sock, auto pbufs, auto& proc_bytes, auto& iflags, auto pol) {
                return lazy::WSARecv_(sock, pbufs, 1, &proc_bytes, &iflags, pol, nullptr);
            });
        });
    }

    expected<AsyncResult> Socket::write_async(view::rvec buffer, void* c, stream_notify_t notify, std::uint32_t flag) {
        return get_tbl(ctx).and_then([&](WinSockTable* t) {
            return async_stream_operation(t, buffer, flag, c, notify, true, call_stream, [](auto sock, auto pbufs, auto& proc_bytes, auto& iflags, auto pol) {
                return lazy::WSASend_(sock, pbufs, 1, &proc_bytes, iflags, pol, nullptr);
            });
        });
    }

    expected<AsyncResult> Socket::readfrom_async(view::wvec buffer, NetAddrPort& addr, void* c, recvfrom_notify_t notify, std::uint32_t flag) {
        return get_tbl(ctx).and_then([&](WinSockTable* t) {
            return async_packet_operation(
                       t, buffer, flag, c, notify, [&](WinSockIOTableHeader& hdr) -> std::pair<sockaddr*, int*> {
                           auto& tbl = static_cast<WinSockReadTable&>(hdr);
                           tbl.from_len = sizeof(tbl.from);
                           return std::make_pair((sockaddr*)&tbl.from, &tbl.from_len); },
                       false, call_recvfrom, [](auto sock, auto bufs, auto& proc_bytes, auto& iflags, auto addr, auto len, auto pol) { return lazy::WSARecvFrom_(sock, bufs, 1, &proc_bytes, &iflags, addr, len, pol, nullptr); })
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
                    return std::make_pair(ptr, &tmp); },
                true, call_stream,
                [&](auto sock, auto bufs, auto& proc_bytes, auto& iflags, auto addr, auto len, auto pol) {
                    return lazy::WSASendTo_(sock, bufs, 1, &proc_bytes, iflags, addr, *len, pol, nullptr);
                });
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

    expected<void> Socket::connect_ex_loopback_optimize(const NetAddrPort& addr) {
        if (!addr.addr.is_loopback()) {
            return {};  // nop
        }
        return get_raw().and_then([](std::uintptr_t sock) -> expected<void> {
            TCP_INITIAL_RTO_PARAMETERS params;
            params.Rtt = TCP_INITIAL_RTO_UNSPECIFIED_RTT;
            params.MaxSynRetransmissions = TCP_INITIAL_RTO_NO_SYN_RETRANSMISSIONS;
            DWORD out = 0;
            auto res = lazy::WSAIoctl_(sock, SIO_TCP_INITIAL_RTO, &params, sizeof(params), nullptr, 0, &out, nullptr, nullptr);
            if (res != 0) {
                return unexpect(error::Errno());
            }
            return {};
        });
    }

    expected<AsyncResult> Socket::connect_async(const NetAddrPort& addr, void* c, stream_notify_t notify, std::uint32_t flag) {
        if (!lazy::ConnectEx_.find()) {
            return unexpect(error::Error("ConnectEx is not supported in this platform", error::Category::os));
        }
        auto raw_sock = get_tbl(ctx);
        if (!raw_sock) {
            return unexpect(raw_sock.error());
        }
        // see also https://gist.github.com/joeyadams/4158972#file-connectex-bind-cpp-L71
        if (!get_local_addr()) {  // not bound, so bind with any address
            sockaddr_storage st{};
            if (addr.addr.type() != NetAddrType::ipv4 && addr.addr.type() != NetAddrType::ipv6) {
                return unexpect("unsupported address type", error::Category::lib, error::fnet_usage_error);
            }
            auto [a, len] = NetAddrPort_to_sockaddr(&st, addr.addr.type() == NetAddrType::ipv4 ? ipv4_any : ipv6_any);
            if (!a) {
                return unexpect("unsupported address type", error::Category::lib, error::fnet_usage_error);
            }
            auto res = lazy::bind_((*raw_sock)->sock, a, len);
            if (res != 0) {
                return unexpect(error::Errno());
            }
        }
        int tmp = 0;
        return raw_sock.and_then([&](WinSockTable* t) {
            return async_packet_operation(
                t, {}, flag, c, notify,
                [&](WinSockIOTableHeader& hdr) {
                    auto& tbl = static_cast<WinSockWriteTable&>(hdr);
                    auto [ptr, len] = NetAddrPort_to_sockaddr(&tbl.to, addr);
                    tmp = len;
                    return std::make_pair(ptr, &tmp);
                },
                true, call_connect,
                [](auto sock, auto bufs, auto& proc_bytes, auto& iflags, auto addr, auto len, auto pol) {
                    return lazy::ConnectEx_(sock, addr, *len, nullptr, 0, &proc_bytes, pol) ? 0 : 1;  // invert return value
                });
        });
    }

    expected<AcceptAsyncResult<Socket>> Socket::accept_async(NetAddrPort& addr, void* c, accept_notify_t notify, std::uint32_t flag) {
        if (!lazy::AcceptEx_.find()) {
            return unexpect(error::Error("AcceptEx is not supported in this platform", error::Category::os));
        }
        if (!lazy::GetAcceptExSockaddrs_.find()) {
            return unexpect(error::Error("GetAcceptExSockaddrs is not supported in this platform", error::Category::os));
        }
        auto attr = get_sockattr();
        if (!attr) {
            return unexpect(attr.error());
        }
        auto to_be_accepted = socket_platform(*attr);
        if (!to_be_accepted) {
            return unexpect(to_be_accepted.error());
        }
        return get_tbl(ctx).and_then([&](WinSockTable* t) -> expected<AcceptAsyncResult<Socket>> {
            auto res = async_packet_operation(
                t, {}, flag, c, notify,
                [&](WinSockIOTableHeader& hdr) {
                    auto& tbl = static_cast<WinSockReadTable&>(hdr);
                    tbl.from_len = acceptex_least_size;
                    tbl.accept_sock = *to_be_accepted;  // saved and released at call_accept or on error below
                    return std::make_pair((sockaddr*)&tbl.storage, &tbl.from_len);
                },
                false, call_accept,
                [&](auto sock, auto bufs, auto& proc_bytes, auto& iflags, auto addr, auto len, auto pol) {
                    return lazy::AcceptEx_(sock, *to_be_accepted, addr, 0, acceptex_least_size_one, acceptex_least_size_one, &proc_bytes, pol) ? 0 : 1;  // invert return value
                });

            if (!res) {
                sockclose(*to_be_accepted);
                return unexpect(res.error());
            }
            else {
                AcceptAsyncResult<Socket> res2;
                res2.cancel = std::move(res->cancel);
                res2.state = res->state;
                if (res2.state == NotifyState::done) {
                    addr = get_accept_ex_addr(&t->r);
                    auto s = setup_socket(*to_be_accepted, t->event);  // transfer to setup_socket
                    if (!s) {
                        return unexpect(s.error());
                    }
                    res2.socket = std::move(*s);
                }
                return res2;
            }
        });
    }

}  // namespace futils::fnet
