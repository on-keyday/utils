/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
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

        error::Error epollIOCommon(RWAsyncSuite* rw, AsyncSuite* suite, auto sock, const byte* data, size_t len, auto appcb, void* ctx, int flag, bool is_stream) {
            if (!register_fd(suite->sock, rw)) {
                return Errno();
            }
            suite->sock = sock;
            if (!data) {
                suite->boxed = make_storage(len);
            }
            else {
                suite->boxed = make_storage(view::rvec(data, len));
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

        error::Error Socket::read_async(size_t bufsize, void* fnctx, void (*cb)(void*, view::wvec data, bool full, error::Error err), int flag, bool is_stream) {
            return startIO(async_ctx, true, [&](RWAsyncSuite* rw, AsyncSuite* suite) -> std::pair<AsyncState, error::Error> {
                if (auto err = epollIOCommon(rw, suite, sock, nullptr, bufsize, cb, fnctx, flag, is_stream); err) {
                    return {AsyncState::failed, err};
                }
                auto unlock = lock(suite);
                suite->cb = [](AsyncSuite* suite, size_t, error::Error err) {
                    suite->cb = nullptr;
                    const auto s = decr_suite(suite);
                    check_epoll_mask(err);
                    size_t red = 0;
                    auto res = socdl.recv_(suite->sock, suite->boxed.as_char(), suite->boxed.size(), 0);
                    if (res < 0) {
                        set_epoll_or_errno(err);
                    }
                    else {
                        red = res;
                        if (suite->is_stream && res == 0) {
                            set_epoll_or_errno(err, true);
                        }
                    }
                    auto app = (void (*)(void*, view::wvec, bool, error::Error))suite->appcb;
                    suite->on_operation = false;  // clear for reentrant
                    app(suite->ctx, suite->boxed.substr(0, red), suite->boxed.size() == red, std::move(err));
                };
                auto [n, err] = read(suite->boxed, flag, is_stream);
                return handleStart(unlock, err, suite, [&, n = &n] {
                    cb(fnctx, *n,
                       suite->boxed.size() == n->size(), error::none);
                });
            });
        }

        error::Error Socket::readfrom_async(size_t bufsize, void* fnctx, void (*cb)(void*, view::wvec data, bool truncated, error::Error err, NetAddrPort&& addr), int flag, bool is_stream) {
            return startIO(async_ctx, true, [&](RWAsyncSuite* rw, AsyncSuite* suite) -> std::pair<AsyncState, error::Error> {
                if (auto err = epollIOCommon(rw, suite, sock, nullptr, bufsize, cb, fnctx, flag, is_stream); err) {
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
                        suite->sock, suite->boxed.as_char(), suite->boxed.size(), suite->plt.flags,
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
                    auto app = reinterpret_cast<void (*)(void*, view::wvec data, bool truncated, error::Error err, NetAddrPort&& addr)>(suite->appcb);
                    suite->on_operation = false;
                    auto sub = suite->boxed.substr(0, red);
                    app(suite->ctx, sub,
                        suite->boxed.size() == red, std::move(err), sockaddr_to_NetAddrPort(saddr, addrlen));
                };
                auto [n, addr, err] = readfrom(suite->boxed, flag, is_stream);
                return handleStart(unlock, err, suite, [&, n = &n, a = &addr] {
                    cb(fnctx, *n,
                       suite->boxed.size() == *n, error::none,
                       std::move(*a));
                });
            });
        }

        error::Error Socket::write_async(view::rvec src, void* fnctx, void (*cb)(void*, size_t, error::Error err), int flag) {
            return startIO(async_ctx, true, [&](RWAsyncSuite* rw, AsyncSuite* suite) -> std::pair<AsyncState, error::Error> {
                if (src.null()) {
                    return {AsyncState::failed, error::Error(invalid_argument, error::ErrorCategory::validationerr)};
                }
                if (auto err = epollIOCommon(rw, suite, sock, src.data(), src.size(), cb, fnctx, flag, false); err) {
                    return {AsyncState::failed, err};
                }
                auto unlock = lock(suite);
                suite->cb = [](AsyncSuite* suite, size_t, error::Error err) {
                    suite->cb = nullptr;
                    const auto d = decr_suite(suite);
                    check_epoll_mask(err);
                    size_t red = 0;
                    auto res = socdl.send_(suite->sock, suite->boxed.as_char(), suite->boxed.size(), suite->plt.flags);
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
                auto [n, err] = write(suite->boxed, flag);
                return handleStart(unlock, err, suite, [&, n = &n] {
                    cb(fnctx, n->size(), error::none);
                });
            });
        }

        error::Error Socket::writeto_async(view::rvec src, const NetAddrPort& addr, void* fnctx, void (*cb)(void*, size_t, error::Error err), int flag) {
            return startIO(async_ctx, true, [&](RWAsyncSuite* rw, AsyncSuite* suite) -> std::pair<AsyncState, error::Error> {
                if (src.null()) {
                    return {AsyncState::failed, error::Error(invalid_argument, error::ErrorCategory::validationerr)};
                }
                if (auto err = epollIOCommon(rw, suite, sock, src.data(), src.size(), cb, fnctx, flag, false); err) {
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
                    auto res = socdl.sendto_(suite->sock, suite->boxed.as_char(), suite->boxed.size(), suite->plt.flags, addr, addrlen);
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
                auto [n, err] = writeto(addr, suite->boxed, flag);
                return handleStart(unlock, err, suite, [&, n = &n] {
                    cb(fnctx, n->size(), error::none);
                });
            });
        }
    }  // namespace dnet
}  // namespace utils
