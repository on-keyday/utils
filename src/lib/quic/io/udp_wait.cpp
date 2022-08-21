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
                /*
                struct IOWait {
                    CommonIOWait common;
                    Timeout timeout;
                    Reserved reserved;
                    io::IO* r;
                };*/

                Result async_wait(core::QUIC* q, Target& target, IOCallback& cb);

                /*
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
                }*/

                struct TmpBuf {
                    byte data[1500];
                };

                core::QueState io_wait_udp(core::EventLoop* l, core::QUIC* q, IO* r, io::Target& target, Timeout& to, const auto& from_time, TmpBuf& buf, IOCallback& cb) {
                    if (!l) {
                        cb(&target, nullptr, 0, Result{Status::fatal});
                        return core::que_remove;
                    }
                    auto res = r->read_from(&target, buf.data, sizeof(buf.data));
                    if (incomplete(res)) {
                        if (to.until_await > 0 || to.full > 0) {
                            auto now = std::chrono::system_clock::now();
                            auto timeout = std::chrono::duration_cast<std::chrono::microseconds>(now - from_time);
                            if (to.until_await > 0 &&
                                timeout.count() >= to.until_await) {
                                res = async_wait(q, target, cb);
                                if (ok(res)) {
                                    return core::que_remove;
                                }
                            }
                            if (to.full > 0 &&
                                timeout.count() >= to.full) {
                                cb(&target, nullptr, 0, Result{Status::timeout});
                                return core::que_remove;
                            }
                        }
                        return core::que_enque;
                    }
                    cb(&target, buf.data, res.len, Result{Status::ok});
                    return core::que_remove;
                }

                dll(Result) enque_io_wait(core::QUIC* q, io::Target t, io::IO* r, tsize, Timeout timeout, IOCallback next) {
                    core::Event ev;
                    ev.type = core::EventType::io_wait;
                    auto now = std::chrono::system_clock::now();
                    ev.callback = core::make_event_cb(&q->g->alloc, [=, buf = TmpBuf{}, self = q->self, n = std::move(next)](core::EventLoop* l) mutable {
                        return io_wait_udp(l, self.get(), r, t, timeout, now, buf, n);
                    });
                    if (!ev.callback) {
                        return {io::Status::resource_exhausted};
                    }
                    core::enque_event(
                        q,
                        std::move(ev));
                    return {io::Status::ok};
                }

#ifdef _WIN32
                struct AsyncBuf {
                    ::OVERLAPPED ol;
                    Target target;
                    ::WSABUF wb;
                    ::sockaddr_storage from;
                    int from_len;
                    ::DWORD flag;
                    mem::shared_ptr<core::QUIC> q;
                    IOCallback cb;
                    TmpBuf buf;
                };

                core::QueState io_completion_wait(core::EventLoop* l, void* iocp) {
                    ::OVERLAPPED* pol = nullptr;
                    ::ULONG_PTR _;
                    ::DWORD transferred;
                    auto res = external::kernel32.GetQueuedCompletionStatus_(iocp, &transferred, &_, &pol, 1);
                    if (pol) {
                        auto b = reinterpret_cast<AsyncBuf*>(pol);
                        Result rres;
                        if (res) {
                            rres = {io::Status::ok, transferred};
                            if (!sockaddr_to_target(&b->from, &b->target)) {
                                rres = {io::Status::failed, transferred, true};
                            }
                        }
                        else {
                            rres = {io::Status::failed, (ErrorCode)GET_ERROR()};
                        }
                        b->cb(&b->target, b->buf.data, transferred, rres);
                        auto g = b->q->g;
                        g->alloc.deallocate(b);
                        return core::que_remove;
                    }
                    return core::que_enque;
                }

                Result async_wait(core::QUIC* q, Target& target, IOCallback& cb) {
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
                    buf->q = q->self;
                    buf->target = target;
                    buf->wb.buf = topchar(buf->buf.data);
                    buf->wb.len = sizeof(buf->buf);
                    buf->from_len = sizeof(buf->from);
                    buf->cb = std::move(cb);
                    // start recv
                    auto err = external::sockdll.WSARecvFrom_(
                        buf->target.target, &buf->wb, 1, nullptr, &buf->flag,
                        repaddr(&buf->from), &buf->from_len, &buf->ol, nullptr);
                    if (err == -1) {
                        auto code = GET_ERROR();
                        if (!ASYNC(code)) {
                            cb = std::move(buf->cb);
                            q->g->alloc.deallocate(buf);
                            return with_code(code);
                        }
                    }
                    core::Event ev;
                    ev.callback = core::make_event_cb(&q->g->alloc, [=, self = q](core::EventLoop* l) {
                        return io_completion_wait(l, iocp);
                    });
                    core::enque_event(q, std::move(ev));
                    return {io::Status::ok};
                }
#else
                Result async_wait(core::QUIC* q, Target& target, IOCallback& cb) {
                    return {io::Status::failed, invalid, true};
                }
#endif
                /*
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
                        q->g->alloc.deallocate(async_io);
                        return;
                    }
                }*/
                /*
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
                }*/
            }  // namespace udp

            io::AsyncHandle get_async_handle(core::QUIC* q) {
#ifdef _WIN32
                if (!external::load_Kernel32()) {
                    return nullptr;
                }
                q->io->iniasync.Do([&] {
                    q->io->async_handle = external::kernel32.CreateIoCompletionPort_(INVALID_HANDLE_VALUE, nullptr, 0, 0);
                });
                return q->io->async_handle;
#else
                return nullptr;
#endif
            }

            dll(io::Result) register_target(core::QUIC* q, io::TargetID id) {
#ifdef _WIN32
                auto iocp = get_async_handle(q);
                if (!iocp) {
                    return {io::Status::failed, io::invalid, true};
                }
                auto err = external::kernel32.CreateIoCompletionPort_(reinterpret_cast<void*>(id), iocp, 0, 0);
                if (err == nullptr) {
                    return {io::Status::failed, (io::ErrorCode)GET_ERROR()};
                }
                return {io::Status::ok};
#else
                return {io::Status::failed, io::invalid, true};
#endif
            }
        }  // namespace io

    }  // namespace quic
}  // namespace utils
