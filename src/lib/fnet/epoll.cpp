/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/dll/errno.h>
#include <fnet/dll/lazy/sockdll.h>
#include <fnet/socket.h>
#include <atomic>
#include <fnet/dll/asyncbase.h>
#include <cstddef>

namespace utils {
    namespace fnet {

        int get_epoll() {
            static auto epol = lazy::epoll_create1_(EPOLL_CLOEXEC);
            return epol;
        }

        int wait_event_plt(std::uint32_t time) {
            int t = time;
            epoll_event ev[64]{};
            sigset_t set;
            auto res = lazy::epoll_pwait_(get_epoll(), ev, 64, t, &set);
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
            return lazy::epoll_ctl_(get_epoll(), EPOLL_CTL_ADD, fd, &ev) == 0 ||
                   get_error() == EEXIST;
        }

        void remove_fd(std::uintptr_t fd) {
            lazy::epoll_ctl_(get_epoll(), EPOLL_CTL_DEL, fd, nullptr);
        }

        error::Error epollIOCommon(RWAsyncSuite* rw, AsyncSuite* suite, std::uintptr_t sock, view::rvec buf, std::shared_ptr<thread::Waker>& waker, int flag, bool is_stream) {
            if (!register_fd(suite->sock, rw)) {
                return error::Errno();
            }
            suite->sock = sock;
            suite->waker = std::move(waker);
            suite->plt.flags = flag;
            suite->plt.buf = buf;
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
                strutil::append(pb, "epoll: error mask ");
                bool flag = false;
                auto write = [&](auto str) {
                    if (!flag) {
                        strutil::append(pb, " | ");
                        flag = true;
                    }
                    strutil::append(pb, str);
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
                eperr->syserr = eof ? error::eof : error::Errno();
            }
            else {
                err = eof ? error::eof : error::Errno();
            }
        }

        [[nodiscard]] auto decr_suite(AsyncSuite* suite) {
            return helper::defer([=] {
                suite->decr();
            });
        }

        error::Error Socket::set_skipnotify(bool skip_notif, bool skip_event) {
            return error::Error("not supported on linux", error::ErrorCategory::fneterr);
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

        view::wvec get_wbuf(AsyncSuite* suite) {
            view::rvec tmp = suite->plt.buf;
            return view::wvec(const_cast<byte*>(tmp.data()), tmp.size());
        }

        error::Error Socket::read_async(view::wvec buf, std::shared_ptr<thread::Waker> waker, int flag, bool is_stream) {
            if (buf.null()) {
                return error::Error("buf MUST NOT be null", error::ErrorCategory::validationerr);
            }
            return startIO(async_ctx, true, [&](RWAsyncSuite* rw, AsyncSuite* suite) -> std::pair<AsyncState, error::Error> {
                if (auto err = epollIOCommon(rw, suite, sock, buf, waker, flag, is_stream); err) {
                    return {AsyncState::failed, err};
                }
                auto unlock = lock(suite);
                suite->cb = [](AsyncSuite* suite, size_t, error::Error err) {
                    suite->cb = nullptr;
                    const auto s = decr_suite(suite);
                    check_epoll_mask(err);
                    size_t red = 0;
                    auto buf = get_wbuf(suite);
                    auto res = lazy::recv_(suite->sock, buf.as_char(), buf.size(), 0);
                    if (res < 0) {
                        set_epoll_or_errno(err);
                    }
                    else {
                        red = res;
                        if (suite->is_stream && res == 0) {
                            set_epoll_or_errno(err, true);
                        }
                    }
                    auto w = std::move(suite->waker);
                    internal::WakerArg arg;
                    arg.size = red;
                    arg.err = std::move(err);
                    suite->on_operation = false;  // clear for reentrant
                    w->wakeup(&arg);
                };
                auto [n, err] = read(buf, flag, is_stream);
                return handleStart(unlock, err, suite, [&, n = &n] {
                    internal::WakerArg arg;
                    arg.size = n->size();
                    auto w = std::move(suite->waker);
                    w->wakeup(&arg);
                });
            });
        }

        error::Error Socket::readfrom_async(view::wvec buf, std::shared_ptr<thread::Waker> waker, int flag, bool is_stream) {
            if (buf.null()) {
                return error::Error("buf MUST NOT be null", error::ErrorCategory::validationerr);
            }
            return startIO(async_ctx, true, [&](RWAsyncSuite* rw, AsyncSuite* suite) -> std::pair<AsyncState, error::Error> {
                if (auto err = epollIOCommon(rw, suite, sock, buf, waker, flag, is_stream); err) {
                    return {AsyncState::failed, err};
                }
                auto unlock = lock(suite);
                suite->cb = [](AsyncSuite* suite, size_t, error::Error err) {
                    const auto s = decr_suite(suite);
                    suite->cb = nullptr;
                    check_epoll_mask(err);
                    sockaddr_storage addr{};
                    socklen_t addrlen = sizeof(addr);
                    size_t red = 0;
                    auto saddr = reinterpret_cast<sockaddr*>(&addr);
                    auto buf = get_wbuf(suite);
                    auto res = lazy::recvfrom_(
                        suite->sock, buf.as_char(), buf.size(), suite->plt.flags,
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
                    internal::WakerArg arg;
                    arg.size = red;
                    arg.err = std::move(err);
                    arg.addr = sockaddr_to_NetAddrPort(saddr, addrlen);
                    auto w = std::move(suite->waker);
                    suite->on_operation = false;
                    w->wakeup(&arg);
                };
                auto [n, addr, err] = readfrom(buf, flag, is_stream);
                return handleStart(unlock, err, suite, [&, n = &n, a = &addr] {
                    internal::WakerArg arg;
                    arg.size = n->size();
                    arg.addr = std::move(*a);
                    auto w = std::move(suite->waker);
                    w->wakeup(&arg);
                });
            });
        }

        error::Error Socket::write_async(view::rvec buf, std::shared_ptr<thread::Waker> waker, int flag) {
            if (buf.null()) {
                return error::Error("buf MUST NOT be null", error::ErrorCategory::validationerr);
            }
            return startIO(async_ctx, true, [&](RWAsyncSuite* rw, AsyncSuite* suite) -> std::pair<AsyncState, error::Error> {
                if (auto err = epollIOCommon(rw, suite, sock, buf, waker, flag, false); err) {
                    return {AsyncState::failed, err};
                }
                auto unlock = lock(suite);
                suite->cb = [](AsyncSuite* suite, size_t, error::Error err) {
                    suite->cb = nullptr;
                    const auto d = decr_suite(suite);
                    check_epoll_mask(err);
                    size_t red = 0;
                    view::rvec buf = suite->plt.buf;
                    auto res = lazy::send_(suite->sock, buf.as_char(), buf.size(), suite->plt.flags);
                    if (res < 0) {
                        set_epoll_or_errno(err);
                    }
                    else {
                        red = res;
                    }
                    internal::WakerArg arg;
                    arg.size = red;
                    arg.err = std::move(err);
                    auto w = std::move(suite->waker);
                    suite->on_operation = false;
                    w->wakeup(&arg);
                };
                auto [n, err] = write(buf, flag);
                return handleStart(unlock, err, suite, [&, n = &n] {
                    internal::WakerArg arg;
                    arg.size = buf.size() - n->size();
                    auto w = std::move(suite->waker);
                    w->wakeup(&arg);
                });
            });
        }

        error::Error Socket::writeto_async(view::rvec buf, const NetAddrPort& addr, std::shared_ptr<thread::Waker> waker, int flag) {
            if (buf.null()) {
                return error::Error("buf MUST NOT be null", error::ErrorCategory::validationerr);
            }
            return startIO(async_ctx, true, [&](RWAsyncSuite* rw, AsyncSuite* suite) -> std::pair<AsyncState, error::Error> {
                if (auto err = epollIOCommon(rw, suite, sock, buf, waker, flag, false); err) {
                    return {AsyncState::failed, err};
                }
                auto unlock = lock(suite);
                suite->cb = [](AsyncSuite* suite, size_t, error::Error err) {
                    suite->cb = nullptr;
                    const auto d = decr_suite(suite);
                    check_epoll_mask(err);
                    size_t red = 0;
                    sockaddr_storage st;
                    internal::WakerArg arg;
                    auto [addr, addrlen] = NetAddrPort_to_sockaddr(&st, suite->plt.addr);
                    if (!addr) {
                        arg.err = error::memory_exhausted;
                        auto w = std::move(suite->waker);
                        // ??? detect error or library bug?
                        suite->on_operation = false;
                        w->wakeup(&arg);
                        return;
                    }
                    view::rvec buf = suite->plt.buf;
                    auto res = lazy::sendto_(suite->sock, buf.as_char(), buf.size(), suite->plt.flags, addr, addrlen);
                    if (res < 0) {
                        set_epoll_or_errno(err);
                    }
                    else {
                        red = res;
                    }
                    arg.err = std::move(err);
                    arg.size = red;
                    auto w = std::move(suite->waker);
                    suite->on_operation = false;
                    w->wakeup(&arg);
                };
                suite->plt.addr = addr;  // copy address
                auto [n, err] = writeto(addr, buf, flag);
                return handleStart(unlock, err, suite, [&, n = &n] {
                    internal::WakerArg arg;
                    arg.size = buf.size() - n->size();
                    auto w = std::move(suite->waker);
                    w->wakeup(&arg);
                });
            });
        }

        // TODO(on-keyday): implement cancel operation
        bool Socket::cancel_io(bool is_read) {
            return false;
        }
    }  // namespace fnet
}  // namespace utils
