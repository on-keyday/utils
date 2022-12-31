/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/errno.h>
#include <dnet/dll/sockdll.h>
#include <dnet/socket.h>
#include <dnet/errcode.h>
#include <atomic>
#include <dnet/dll/asyncbase.h>
#include <cstddef>

namespace utils {
    namespace dnet {
        /*
        Socket make_socket(std::uintptr_t);
        struct AsyncBuffer {
            AsyncBufferCommon common;
            std::atomic_uint32_t ref;
            std::uintptr_t sock;
            ::sockaddr_storage storage;
            void incref() {
                ref++;
            }
            void decref() {
                if (ref.fetch_sub(1) == 1) {
                    delete_with_global_heap(this, DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(AsyncBuffer)));
                }
            }
            static void start(AsyncHead* h, std::uintptr_t sock) {
                auto buf = reinterpret_cast<AsyncBuffer*>(h);
                buf->incref();
                buf->sock = sock;
            }
            static void completion(AsyncHead* h, size_t size) {
                auto buf = reinterpret_cast<AsyncBuffer*>(h);
                auto& ebuf = buf->common.ebuf;
                auto& cmp = buf->common.comp;
                auto user = buf->common.user;
                int res = 0;
                auto storage = reinterpret_cast<sockaddr*>(&buf->storage);
                bool do_completion = cmp.user_completion != nullptr;
                if (h->method == am_recv) {
                    res = socdl.recv_(buf->sock, ebuf.buf, ebuf.size, 0);
                    if (res < 0) {
                        res = 0;
                    }
                    if (do_completion) {
                        cmp.user_completion(user, ebuf.buf, res, ebuf.size, get_error());
                    }
                }
                else if (h->method == am_recvfrom) {
                    socklen_t len = sizeof(buf->storage);
                    res = socdl.recvfrom_(buf->sock, ebuf.buf, ebuf.size, 0, storage, &len);
                    if (res < 0) {
                        res = 0;  // ignore error
                    }
                    if (do_completion) {
                        cmp.user_completion_from(user, ebuf.buf, res, ebuf.size, &buf->storage, len, get_error());
                    }
                }
                else if (h->method == am_accept) {
                    socklen_t len = sizeof(buf->storage);
                    res = socdl.accept_(buf->sock, storage, &len);
                    set_nonblock(res, true);
                    if (do_completion) {
                        cmp.user_completion_accept(user, make_socket(std::uintptr_t(res)), get_error());
                    }
                }
                else if (h->method == am_connect) {
                    if (do_completion) {
                        cmp.user_completion_connect(user, get_error());
                    }
                }
                else if (h->method == am_send) {
                    socdl.send_(buf->sock, ebuf.buf, ebuf.size, 0);
                    if (do_completion) {
                        cmp.user_completion_connect(user, get_error());
                    }
                }
                buf->decref();
            }
            static void canceled(AsyncHead* h) {
                reinterpret_cast<AsyncBuffer*>(h)->decref();
            }
            AsyncBuffer()
                : ref(1) {
                common.head.start = start;
                common.head.completion = completion;
                common.head.canceled = canceled;
            }
        };
        AsyncBuffer* AsyncBuffer_new() {
            return new_from_global_heap<AsyncBuffer>(DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(AsyncBuffer)));
        }

        void remove_fd(int fd);

        void AsyncBuffer_gc(void* opt, std::uintptr_t sock) {
            remove_fd(sock);
            static_cast<AsyncBuffer*>(opt)->decref();
        }

        int get_epoll() {
            static auto epol = socdl.epoll_create1_(EPOLL_CLOEXEC);
            return epol;
        }

        bool register_fd(int fd, void* ptr) {
            epoll_event ev{};
            ev.events = 0;
            ev.data.ptr = ptr;
            return socdl.epoll_ctl_(get_epoll(), EPOLL_CTL_ADD, fd, &ev) == 0 ||
                   get_error() == EEXIST;
        }

        void remove_fd(int fd) {
            socdl.epoll_ctl_(get_epoll(), EPOLL_CTL_DEL, fd, nullptr);
        }

        bool watch_event(int fd, std::uint32_t event, void* ptr) {
            epoll_event ev{};
            ev.events = event;
            ev.data.ptr = ptr;
            return socdl.epoll_ctl_(get_epoll(), EPOLL_CTL_MOD, fd, &ev) == 0;
        }



        bool start_async_operation(
            void* ptr, std::uintptr_t sock, AsyncMethod mode,
            *for connectex
            const void* addr, size_t addrlen) {
            if (!register_fd(sock, ptr)) {
                return false;
            }
            auto head = static_cast<AsyncHead*>(ptr);
            if (head->reserved != async_reserved || !head->completion) {
                set_error(invalid_async_head);
                return false;
            }
            head->method = mode;
            if (head->start) {
                head->start(head, sock);
            }
            auto cancel = [&] {
                if (head->canceled) {
                    head->canceled(head);
                }
            };
            constexpr auto edge_trigger = EPOLLONESHOT | EPOLLET | EPOLLEXCLUSIVE;
            if (mode == AsyncMethod::am_recv || mode == AsyncMethod::am_recvfrom ||
                mode == AsyncMethod::am_accept) {
                if (!watch_event(sock, EPOLLIN | edge_trigger, ptr)) {
                    cancel();
                    return false;
                }
            }
            else if (mode == AsyncMethod::am_connect) {
                if (!watch_event(sock, EPOLLOUT | edge_trigger, ptr)) {
                    cancel();
                    return false;
                }
                auto res = socdl.connect_(sock, static_cast<const sockaddr*>(addr), addrlen);
                if (res != 0 || (get_error() != EWOULDBLOCK && get_error() != EINPROGRESS)) {
                    cancel();
                    return false;
                }
            }
            else if (mode == AsyncMethod::am_send || mode == AsyncMethod::am_sendto) {
                if (!watch_event(sock, EPOLLOUT | edge_trigger, ptr)) {
                    cancel();
                    return false;
                }
            }
            return true;
        }
        */

        int get_epoll() {
            static auto epol = socdl.epoll_create1_(EPOLL_CLOEXEC);
            return epol;
        }

        int wait_event_plt(std::uint32_t time) {
            int t = time;
            epoll_event ev[64]{};
            sigset_t set;
            auto res = socdl.epoll_pwait_(get_epoll(), ev, 64, t, &set);
            if (res == -1) {
                return 0;
            }
            int proc = 0;
            for (auto i = 0; i < res; i++) {
                auto h = (RWAsyncSuite*)ev[i].data.ptr;
                if (!h) {
                    continue;
                }
                h->incr();  // keep alive
                const auto r = helper::defer([&] {
                    h->decr();
                });
                auto events = ev[i].events;
                if (events & (EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                    if (h->r && h->r->cb) {
                        while (h->r->plt.lock) {
                            timespec spec;
                            spec.tv_sec = 0;
                            spec.tv_nsec = 1;
                            nanosleep(&spec, &spec);
                        }
                        if (h->r->cb) {
                            h->r->cb(h->r, 0, error::Error(events, error::ErrorCategory::noerr));
                        }
                    }
                }
                if (events & (EPOLLOUT | EPOLLERR | EPOLLHUP)) {
                    if (h->w && h->w->cb) {
                        while (h->w->plt.lock) {
                            timespec spec;
                            spec.tv_sec = 0;
                            spec.tv_nsec = 1;
                            nanosleep(&spec, &spec);
                        }
                        if (h->w->cb) {
                            h->w->cb(h->w, 0, error::Error(events, error::ErrorCategory::noerr));
                        }
                    }
                }
                proc++;
            }
            return proc;
        }

        bool register_fd(int fd, void* ptr) {
            epoll_event ev{};
            ev.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLEXCLUSIVE;
            ev.data.ptr = ptr;
            set_error(0);
            return socdl.epoll_ctl_(get_epoll(), EPOLL_CTL_ADD, fd, &ev) == 0 ||
                   get_error() == EEXIST;
        }

        void remove_fd(std::uintptr_t fd) {
            socdl.epoll_ctl_(get_epoll(), EPOLL_CTL_DEL, fd, nullptr);
        }

        error::Error epollIOCommon(RWAsyncSuite* rw, AsyncSuite* suite, auto sock, ConstByteLen buf, auto appcb, void* ctx, int flag, bool is_stream) {
            if (!register_fd(suite->sock, rw)) {
                return Errno();
            }
            suite->sock = sock;
            if (!buf.data) {
                suite->boxed = make_storage(buf.len);
            }
            else {
                suite->boxed = make_storage(buf.data, buf.len);
            }
            if (suite->boxed.null()) {
                return error::memory_exhausted;
            }
            suite->appcb = reinterpret_cast<void (*)()>(appcb);
            suite->ctx = ctx;
            suite->plt.flags = flag;
            suite->is_stream = is_stream;
            return error::none;
        }

        // lock to protect from execution race
        // between wait_event_plt and invoker function
        [[nodiscard]] auto lock(AsyncSuite* suite) {
            suite->plt.lock = true;
            return helper::defer([=] {
                suite->plt.lock = false;
            });
        }

        struct EpollError {
            std::uint32_t events;
            error::Error syserr;

            void error(auto&& pb) {
                helper::append(pb, "epoll: error mask ");
                bool flag = false;
                auto write = [&](auto str) {
                    if (!flag) {
                        helper::append(pb, " | ");
                        flag = true;
                    }
                    helper::append(pb, str);
                };
                if (events & EPOLLERR) {
                    write("EPOLLERR");
                }
                if (events & EPOLLHUP) {
                    write("EPOLLHUP");
                }
                if (events & EPOLLRDHUP) {
                    write("EPOLLRDHUP");
                }
            }

            error::Error unwrap() {
                return syserr;
            }

            error::ErrorCategory category() {
                return error::ErrorCategory::syserr;
            }
        };

        void check_epoll_mask(error::Error& err) {
            auto errmask = err.errnum() & (EPOLLHUP | EPOLLERR);
            if (errmask) {
                err = error::Error(EpollError{
                    .events = std::uint32_t(errmask),
                });
            }
            else {
                err = error::none;
            }
        }

        void set_epoll_or_errno(error::Error& err, bool eof = false) {
            if (auto eperr = err.as<EpollError>()) {
                eperr->syserr = eof ? error::eof : Errno();
            }
            else {
                err = eof ? error::eof : Errno();
            }
        }

        [[nodiscard]] auto decr_suite(AsyncSuite* suite) {
            return helper::defer([=] {
                suite->decr();
            });
        }

        error::Error Socket::set_skipnotify(bool skip_notif, bool skip_event) {
            return error::Error(not_supported, error::ErrorCategory::dneterr);
        }

        std::pair<AsyncState, error::Error> handleStart(auto& unlock, error::Error& err, AsyncSuite* suite, auto&& cb) {
            if (err) {
                if (err.category() == error::ErrorCategory::syserr &&
                    err.errnum() == EAGAIN) {
                    return {AsyncState::started, error::none};
                }
                return {AsyncState::failed, err};
            }
            suite->cb = nullptr;
            unlock.execute();
            suite->on_operation = false;  // clear for rentrant
            cb();
            return {AsyncState::completed, error::none};
        }

        error::Error Socket::read_async(size_t bufsize, void* fnctx, void (*cb)(void*, ByteLen data, bool full, error::Error err), int flag, bool is_stream) {
            return startIO(async_ctx, true, [&](RWAsyncSuite* rw, AsyncSuite* suite) -> std::pair<AsyncState, error::Error> {
                if (auto err = epollIOCommon(rw, suite, sock, {nullptr, bufsize}, cb, fnctx, flag, is_stream); err) {
                    return {AsyncState::failed, err};
                }
                auto unlock = lock(suite);
                suite->cb = [](AsyncSuite* suite, size_t, error::Error err) {
                    suite->cb = nullptr;
                    const auto s = decr_suite(suite);
                    check_epoll_mask(err);
                    size_t red = 0;
                    auto res = socdl.recv_(suite->sock, suite->boxed.data(), suite->boxed.len(), 0);
                    if (res < 0) {
                        set_epoll_or_errno(err);
                    }
                    else {
                        red = res;
                        if (suite->is_stream && res == 0) {
                            set_epoll_or_errno(err, true);
                        }
                    }
                    auto app = (void (*)(void*, ByteLen, bool, error::Error))suite->appcb;
                    suite->on_operation = false;  // clear for reentrant
                    app(suite->ctx, suite->boxed.unbox().resized(red), suite->boxed.len() == red, std::move(err));
                };
                auto [n, err] = read(suite->boxed.data(), suite->boxed.len(), flag, is_stream);
                return handleStart(unlock, err, suite, [&, n = &n] {
                    cb(fnctx, suite->boxed.unbox().resized(*n),
                       suite->boxed.len() == *n, error::none);
                });
            });
        }

        error::Error Socket::readfrom_async(size_t bufsize, void* fnctx, void (*cb)(void*, ByteLen data, bool truncated, error::Error err, NetAddrPort&& addr), int flag, bool is_stream) {
            return startIO(async_ctx, true, [&](RWAsyncSuite* rw, AsyncSuite* suite) -> std::pair<AsyncState, error::Error> {
                if (auto err = epollIOCommon(rw, suite, sock, {nullptr, bufsize}, cb, fnctx, flag, is_stream); err) {
                    return {AsyncState::failed, err};
                }
                auto unlock = lock(suite);
                suite->cb = [](AsyncSuite* suite, size_t, error::Error err) {
                    suite->cb = nullptr;
                    check_epoll_mask(err);
                    sockaddr_storage addr{};
                    socklen_t addrlen = sizeof(addr);
                    size_t red = 0;
                    auto saddr = reinterpret_cast<sockaddr*>(&addr);
                    auto res = socdl.recvfrom_(
                        suite->sock, suite->boxed.data(), suite->boxed.len(), suite->plt.flags,
                        saddr, &addrlen);
                    if (res < 0) {
                        set_epoll_or_errno(err);
                    }
                    else {
                        red = res;
                        if (suite->is_stream && res == 0) {
                            set_epoll_or_errno(err, true);
                        }
                    }
                    const auto s = helper::defer([&] {
                        suite->decr();
                    });
                    auto app = reinterpret_cast<void (*)(void*, ByteLen data, bool truncated, error::Error err, NetAddrPort&& addr)>(suite->appcb);
                    suite->on_operation = false;
                    auto sub = suite->boxed.substr(0, red);
                    app(suite->ctx, ByteLen{sub.data(), sub.size()},
                        suite->boxed.size() == red, std::move(err), sockaddr_to_NetAddrPort(saddr, addrlen));
                };
                sockaddr_storage addr{};
                int addrlen = sizeof(addr);
                auto [n, err] = readfrom((dnet::raw_address*)&addr, &addrlen, suite->boxed.data(), suite->boxed.len(), flag, is_stream);
                return handleStart(unlock, err, suite, [&, n = &n] {
                    auto sub = suite->boxed.substr(0, *n);
                    cb(fnctx, ByteLen{sub.data(), sub.size()},
                       suite->boxed.size() == *n, error::none,
                       sockaddr_to_NetAddrPort(reinterpret_cast<sockaddr*>(&addr), addrlen));
                });
            });
        }

        error::Error Socket::write_async(ConstByteLen src, void* fnctx, void (*cb)(void*, size_t, error::Error err), int flag) {
            return startIO(async_ctx, true, [&](RWAsyncSuite* rw, AsyncSuite* suite) -> std::pair<AsyncState, error::Error> {
                if (!src.data) {
                    return {AsyncState::failed, error::Error(invalid_argument, error::ErrorCategory::validationerr)};
                }
                if (auto err = epollIOCommon(rw, suite, sock, src, cb, fnctx, flag, false); err) {
                    return {AsyncState::failed, err};
                }
                auto unlock = lock(suite);
                suite->cb = [](AsyncSuite* suite, size_t, error::Error err) {
                    suite->cb = nullptr;
                    const auto d = decr_suite(suite);
                    check_epoll_mask(err);
                    size_t red = 0;
                    auto res = socdl.send_(suite->sock, suite->boxed.data(), suite->boxed.len(), suite->plt.flags);
                    if (res < 0) {
                        set_epoll_or_errno(err);
                    }
                    else {
                        red = res;
                    }
                    auto app = reinterpret_cast<void (*)(void*, size_t, error::Error)>(suite->appcb);
                    suite->on_operation = false;
                    app(suite->ctx, red, std::move(err));
                };
                auto [n, err] = write(suite->boxed.data(), suite->boxed.len(), flag);
                return handleStart(unlock, err, suite, [&, n = &n] {
                    cb(fnctx, *n, error::none);
                });
            });
        }

        error::Error Socket::writeto_async(ConstByteLen src, const NetAddrPort& addr, void* fnctx, void (*cb)(void*, size_t, error::Error err), int flag) {
            return startIO(async_ctx, true, [&](RWAsyncSuite* rw, AsyncSuite* suite) -> std::pair<AsyncState, error::Error> {
                if (!src.data) {
                    return {AsyncState::failed, error::Error(invalid_argument, error::ErrorCategory::validationerr)};
                }
                if (auto err = epollIOCommon(rw, suite, sock, src, cb, fnctx, flag, false); err) {
                    return {AsyncState::failed, err};
                }
                auto unlock = lock(suite);
                suite->cb = [](AsyncSuite* suite, size_t, error::Error err) {
                    suite->cb = nullptr;
                    const auto d = decr_suite(suite);
                    check_epoll_mask(err);
                    size_t red = 0;
                    sockaddr_storage st;
                    auto [addr, addrlen] = NetAddrPort_to_sockaddr(&st, suite->plt.addr);
                    auto app = reinterpret_cast<void (*)(void*, size_t, error::Error)>(suite->appcb);
                    if (!addr) {
                        // ??? detect error or library bug?
                        suite->on_operation = false;
                        app(suite->ctx, 0, error::memory_exhausted);
                        return;
                    }
                    auto res = socdl.sendto_(suite->sock, suite->boxed.data(), suite->boxed.len(), suite->plt.flags, addr, addrlen);
                    if (res < 0) {
                        set_epoll_or_errno(err);
                    }
                    else {
                        red = res;
                    }
                    suite->on_operation = false;
                    app(suite->ctx, red, std::move(err));
                };
                suite->plt.addr = addr;  // copy address
                auto [n, err] = writeto(addr, suite->boxed.data(), suite->boxed.len(), flag);
                return handleStart(unlock, err, suite, [&, n = &n] {
                    cb(fnctx, *n, error::none);
                });
            });
        }
    }  // namespace dnet
}  // namespace utils
