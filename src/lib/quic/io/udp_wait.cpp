/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <quic/common/dll_cpp.h>
#include <quic/core/core.h>
#include <quic/io/io.h>
#include <quic/io/udp.h>
#include <quic/internal/context_internal.h>
#include <quic/internal/sock_internal.h>
#include <quic/mem/once.h>
#include <quic/internal/external_func_internal.h>

// timeout
#include <chrono>

namespace utils {
    namespace quic {
        namespace io {
            namespace udp {
                using Reserved = uint;
                constexpr Reserved reserved = 0x4F'E6'2A'69;

                struct IOWait {
                    CommonIOWait common;
                    Timeout timeout;
                    Reserved reserved;
                    io::IO* r;
                };

                Result async_wait(core::QUIC* q, CommonIOWait& common, Timeout time);

                core::QueState io_wait_udp_except(core::EventLoop* l, core::QUIC* q, core::EventArg arg) {
                    if (!arg.ptr) {
                        return core::que_remove;
                    }
                    auto iow = arg.to<IOWait>();
                    if (!l) {
                        iow->common.next.callback(nullptr, q, iow->common.next.arg);
                        iow->common.next.arg.type = core::EventType::idle;
                    }
                    q->g->alloc.discard_buffer(iow->common.b);
                    auto next = iow->common.next;
                    q->g->alloc.deallocate(iow);
                    if (next.arg.type != core::EventType::idle) {
                        core::enque_event(q, next);
                        return core::que_enque;
                    }
                    return core::que_remove;
                }

                core::QueState io_wait_udp(core::EventLoop* l, core::QUIC* q, core::EventArg arg) {
                    if (!arg.ptr) {
                        return core::que_remove;
                    }
                    if (!l) {
                        return io_wait_udp_except(l, q, arg);
                    }
                    auto iow = arg.to<IOWait>();
                    auto datap = iow->common.b.own();
                    if (!datap) {
                        core::enque_event(q, core::event(core::EventType::memory_exhausted, iow, nullptr, arg.timer, io_wait_udp_except));
                        return core::que_enque;
                    }
                    auto res = iow->r->read_from(&iow->common.target, datap, iow->common.b.size());
                    if (incomplete(res)) {
                        if (iow->timeout.until_await > 0 || iow->timeout.full > 0) {
                            auto now = std::chrono::system_clock::now();
                            auto from = std::chrono::system_clock::from_time_t(arg.timer);
                            auto timeout = std::chrono::duration_cast<std::chrono::microseconds>(now - from);
                            if (iow->timeout.until_await > 0 &&
                                timeout.count() >= iow->timeout.until_await) {
                                res = async_wait(q, iow->common, iow->timeout);
                                if (ok(res)) {
                                    q->g->alloc.deallocate(iow);
                                    return core::que_enque;
                                }
                            }
                            if (iow->timeout.full > 0 &&
                                timeout.count() >= iow->timeout.full) {
                                iow->common.res = {io::Status::timeout, io::invalid, true};
                                iow->common.next.arg.prev = iow;
                                core::enque_event(q, iow->common.next);
                                return core::que_enque;
                            }
                        }
                        core::enque_event(
                            q,
                            core::event(core::EventType::io_wait, iow, nullptr, arg.timer, io_wait_udp));
                        return core::que_enque;
                    }
                    iow->common.res = res;
                    iow->common.next.arg.prev = iow;
                    core::enque_event(q, iow->common.next);
                    return core::que_enque;
                }

                dll(Result) enque_io_wait(core::QUIC* q, io::Target t, io::IO* r, tsize mtu, Timeout timeout, core::Event next) {
                    auto iow = q->g->alloc.allocate<IOWait>();
                    if (!iow) {
                        return {io::Status::resource_exhausted};
                    }
                    iow->reserved = reserved;
                    iow->r = r;
                    iow->common.next = next;
                    iow->common.target = t;
                    iow->timeout = timeout;
                    if (mtu) {
                        iow->common.b = q->g->bpool.get(mtu);
                    }
                    else {
                        iow->common.b = q->g->bpool.get(1500);
                    }
                    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
                    core::enque_event(
                        q,
                        core::event(core::EventType::io_wait, iow, nullptr, now, io_wait_udp));
                    return {io::Status::ok};
                }

#ifdef _WIN32
                struct AsyncBuf {
                    ::OVERLAPPED ol;
                    CommonIOWait common;
                    Reserved reserved;
                    ::WSABUF wb;
                    ::sockaddr_storage from;
                    int from_len;
                    ::DWORD flag;
                    core::QUIC* q;
                };

                core::QueState io_completion_wait(core::EventLoop* l, core::QUIC* q, core::EventArg arg) {
                    ::OVERLAPPED* pol = nullptr;
                    ::ULONG_PTR _;
                    ::DWORD transferred;
                    auto res = external::kernel32.GetQueuedCompletionStatus_(arg.ptr, &transferred, &_, &pol, 1);
                    if (pol) {
                        auto b = reinterpret_cast<AsyncBuf*>(pol);
                        if (res) {
                            b->common.res = {io::Status::ok, transferred};
                            if (!sockaddr_to_target(&b->from, &b->common.target)) {
                                b->common.res = {io::Status::failed, transferred, true};
                            }
                        }
                        else {
                            b->common.res = {io::Status::failed, (ErrorCode)GET_ERROR()};
                        }
                        b->common.next.arg.prev = b;
                        core::enque_event(b->q, b->common.next);
                        return core::que_enque;
                    }
                    core::enque_event(q, {arg, io_completion_wait});
                    return core::que_enque;
                }

                Result async_wait(core::QUIC* q, CommonIOWait& common, Timeout time) {
                    if (!external::sockdll.loaded()) {
                        return {io::Status::fatal, invalid, true};
                    }
                    auto buf = q->g->alloc.allocate<AsyncBuf>();
                    if (!buf) {
                        return {io::Status::resource_exhausted};
                    }
                    auto iocp = get_async_handle(q);
                    if (!iocp) {
                        return {io::Status::resource_exhausted};
                    }
                    buf->q = q;
                    buf->reserved = reserved;
                    buf->common = std::move(common);
                    buf->wb.buf = topchar(buf->common.b.own());
                    buf->wb.len = buf->common.b.size();
                    buf->from_len = sizeof(buf->from);
                    auto err = external::sockdll.WSARecvFrom_(
                        buf->common.target.target, &buf->wb, 1, nullptr, &buf->flag,
                        repaddr(&buf->from), &buf->from_len, &buf->ol, nullptr);
                    if (err == -1) {
                        auto code = GET_ERROR();
                        if (!ASYNC(code)) {
                            common = std::move(buf->common);
                            q->g->alloc.deallocate(buf);
                            return with_code(code);
                        }
                    }
                    core::enque_event(
                        q,
                        core::event(core::EventType::io_wait, iocp, nullptr, 0, io_completion_wait));
                    return {io::Status::ok};
                }

#endif

                dll(void) del_iowait(core::QUIC* q, void* v) {
                    if (!q || !v) {
                        return;
                    }
                    auto iow = static_cast<IOWait*>(v);
                    if (iow->reserved == reserved) {
                        q->g->bpool.put(std::move(iow->common.b));
                        q->g->alloc.deallocate(iow);
                        return;
                    }
                    auto async_io = static_cast<AsyncBuf*>(v);
                    if (async_io->reserved == reserved) {
                        q->g->bpool.put(std::move(async_io->common.b));
                        q->g->alloc.deallocate(async_io);
                        return;
                    }
                }

                dll(CommonIOWait*) get_common_iowait(void* v) {
                    if (!v) {
                        return nullptr;
                    }
                    auto iow = static_cast<IOWait*>(v);
                    if (iow->reserved == reserved) {
                        return &iow->common;
                    }
                    auto async_io = static_cast<AsyncBuf*>(v);
                    if (async_io->reserved == reserved) {
                        return &async_io->common;
                    }
                    return nullptr;
                }
            }  // namespace udp

            io::AsyncHandle get_async_handle(core::QUIC* q) {
                if (!external::load_Kernel32()) {
                    return nullptr;
                }
                q->io->iniasync.Do([&] {
                    q->io->async_handle = external::kernel32.CreateIoCompletionPort_(INVALID_HANDLE_VALUE, nullptr, 0, 0);
                });
                return q->io->async_handle;
            }

            dll(io::Result) register_target(core::QUIC* q, io::TargetID id) {
                auto iocp = get_async_handle(q);
                if (!iocp) {
                    return {io::Status::failed, io::invalid, true};
                }
                auto err = external::kernel32.CreateIoCompletionPort_(reinterpret_cast<void*>(id), iocp, 0, 0);
                if (err == nullptr) {
                    return {io::Status::failed, (io::ErrorCode)GET_ERROR()};
                }
                return {io::Status::ok};
            }
        }  // namespace io

    }  // namespace quic
}  // namespace utils
