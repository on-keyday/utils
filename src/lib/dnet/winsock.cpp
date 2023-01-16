/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/dllcpp.h>
#include <dnet/dll/sockdll.h>
#include <dnet/socket.h>
#include <dnet/errcode.h>
#include <dnet/dll/errno.h>
#include <dnet/dll/asyncbase.h>
#include <dnet/address.h>

namespace utils {
    namespace dnet {

        static auto init_iocp_handle() {
            if (!kerlib.load()) {
                return (void*)nullptr;
            }
            return kerlib.CreateIoCompletionPort_(INVALID_HANDLE_VALUE, NULL,
                                                  0, 0);
        }

        static auto get_handle() {
            static auto iocp = init_iocp_handle();
            return iocp;
        }

        static bool register_handle(std::uintptr_t handle) {
            auto iocp = get_handle();
            if (!iocp) {
                return false;
            }
            set_error(0);
            if (kerlib.CreateIoCompletionPort_((HANDLE)handle, iocp, 0, 0) == nullptr) {
                if (handle == ~0) {
                    return false;  // yes completely wrong
                }
                // TODO(on-keyday):
                // multiple registration is failed with ERROR_INVALID_PARAMETER
                // should we hold flag of registration?
                if (get_error() != ERROR_INVALID_PARAMETER) {
                    return false;
                }
            }
            return true;
        }

        void remove_fd(std::uintptr_t fd) {}

        int wait_event_plt(std::uint32_t time) {
            auto iocp = get_handle();
            if (!iocp) {
                return 0;
            }
            OVERLAPPED_ENTRY ent[64];
            DWORD rem = 0;
            auto res = kerlib.GetQueuedCompletionStatusEx_(iocp, ent, 64, &rem, time, false);
            if (!res) {
                return 0;
            }
            int ev = 0;
            for (auto i = 0; i < rem; i++) {
                auto async_head = (AsyncSuite*)ent[i].lpOverlapped;
                if (!async_head) {
                    continue;
                }
                error::Error err;
                DWORD nt = 0;
                if (!GetOverlappedResult((HANDLE)async_head->sock, ent[i].lpOverlapped, &nt, false)) {
                    err = Errno();
                }
                async_head->cb(async_head, ent[i].dwNumberOfBytesTransferred, std::move(err));
                // async_head->completion(async_head, ent[i].dwNumberOfBytesTransferred);
                ev++;
            }
            return ev;
        }

        error::Error winIOCommon(std::uintptr_t sock, const byte* data, size_t len, AsyncSuite* suite, auto appcb, void* fnctx, int flag, bool is_stream) {
            if (!register_handle(sock)) {
                return Errno();
            }
            suite->sock = sock;
            if (len) {
                if (!data) {
                    suite->boxed = make_storage(len);
                }
                else {
                    suite->boxed = make_storage(view::rvec(data, len));
                }
                if (suite->boxed.null()) {
                    return error::memory_exhausted;
                }
                suite->plt.buf.buf = suite->boxed.as_char();
                suite->plt.buf.len = suite->boxed.size();
            }
            suite->plt.ol = {};
            suite->appcb = reinterpret_cast<void (*)()>(appcb);
            suite->ctx = fnctx;
            suite->is_stream = is_stream;
            suite->plt.flags = flag;
            return error::none;
        }

        error::Error Socket::set_skipnotify(bool skip_notif, bool skip_event) {
            if (!kerlib.SetFileCompletionNotificationModes_) {
                return error::Error(not_supported, error::ErrorCategory::dneterr);
            }
            auto rw = getRWAsyncSuite(async_ctx);
            if (!rw) {
                return error::memory_exhausted;
            }
            byte flag = 0;
            if (skip_event) {
                flag |= FILE_SKIP_SET_EVENT_ON_HANDLE;
            }
            if (skip_notif) {
                flag |= FILE_SKIP_COMPLETION_PORT_ON_SUCCESS;
            }
            if (!kerlib.SetFileCompletionNotificationModes_((HANDLE)sock, flag)) {
                return Errno();
            }
            if (skip_notif) {
                rw->plt.skipNotif = true;
            }
            return error::none;
        }

        std::pair<AsyncState, error::Error> handleStart(RWAsyncSuite* rw, AsyncSuite* suite, auto&& cb) {
            auto result = get_error();
            if (result != NO_ERROR && result != WSA_IO_PENDING) {
                return {AsyncState::failed, Errno()};
            }
            if (rw->plt.skipNotif) {
                suite->on_operation = false;
                cb();
                return {AsyncState::completed, error::none};
            }
            return {AsyncState::started, error::none};
        }

        error::Error Socket::read_async(size_t bufsize, void* fnctx, void (*cb)(void*, view::wvec data, bool full, error::Error err), int flag, bool is_stream) {
            return startIO(async_ctx, true, [&](RWAsyncSuite* rw, AsyncSuite* suite) -> std::pair<AsyncState, error::Error> {
                if (auto err = winIOCommon(sock, nullptr, bufsize, suite, cb, fnctx, flag, is_stream)) {
                    return {AsyncState::failed, err};
                }
                suite->cb = [](AsyncSuite* s, size_t size, error::Error err) {
                    const auto r = helper::defer([&] {
                        s->decr();
                    });
                    auto app = reinterpret_cast<void (*)(void*, view::wvec, bool, error::Error)>(s->appcb);
                    s->on_operation = false;
                    auto sub = s->boxed.substr(0, size);
                    app(s->ctx, sub, size == s->boxed.size(), std::move(err));
                };
                set_error(0);
                socdl.WSARecv_(suite->sock, &suite->plt.buf, 1, &suite->plt.read, &suite->plt.flags, &suite->plt.ol, nullptr);
                return handleStart(rw, suite, [&] {
                    auto sub = suite->boxed.substr(0, suite->plt.read);
                    cb(fnctx, sub,
                       suite->boxed.size() == suite->plt.read, error::none);
                });
            });
        }

        error::Error Socket::readfrom_async(size_t bufsize, void* fnctx, void (*cb)(void*, view::wvec data, bool truncated, error::Error err, NetAddrPort&& addrport), int flag, bool is_stream) {
            return startIO(async_ctx, true, [&](RWAsyncSuite* rw, AsyncSuite* suite) -> std::pair<AsyncState, error::Error> {
                if (auto err = winIOCommon(sock, nullptr, bufsize, suite, cb, fnctx, flag, is_stream)) {
                    return {AsyncState::failed, err};
                }
                suite->cb = [](AsyncSuite* s, size_t size, error::Error err) {
                    const auto r = helper::defer([&] {
                        s->decr();
                    });
                    auto app = reinterpret_cast<void (*)(void*, view::wvec, bool, error::Error, NetAddrPort&&)>(s->appcb);
                    s->on_operation = false;
                    auto sub = s->boxed.substr(0, size);
                    app(s->ctx, sub, size == s->boxed.size(), std::move(err),
                        sockaddr_to_NetAddrPort(reinterpret_cast<sockaddr*>(&s->plt.addr), s->plt.addrlen));
                };
                auto addr = reinterpret_cast<sockaddr*>(&suite->plt.addr);
                suite->plt.addrlen = sizeof(suite->plt.addr);
                suite->plt.ol = {};
                set_error(0);
                socdl.WSARecvFrom_(suite->sock, &suite->plt.buf, 1, &suite->plt.read, &suite->plt.flags,
                                   addr, &suite->plt.addrlen, &suite->plt.ol, nullptr);
                return handleStart(rw, suite, [&] {
                    auto sub = suite->boxed.substr(0, suite->plt.read);
                    cb(fnctx, sub,
                       suite->boxed.size() == suite->plt.read, error::none,
                       sockaddr_to_NetAddrPort(reinterpret_cast<sockaddr*>(&suite->plt.addr), suite->plt.addrlen));
                });
            });
        }

        error::Error Socket::write_async(view::rvec src, void* fnctx, void (*cb)(void*, size_t, error::Error err), int flag) {
            return startIO(async_ctx, false, [&](RWAsyncSuite* rw, AsyncSuite* suite) -> std::pair<AsyncState, error::Error> {
                if (src.null()) {
                    return {AsyncState::failed, error::Error(invalid_argument, error::ErrorCategory::validationerr)};
                }
                if (auto err = winIOCommon(sock, src.data(), src.size(), suite, cb, fnctx, flag, false)) {
                    return {AsyncState::failed, err};
                }
                suite->cb = [](AsyncSuite* s, size_t size, error::Error err) {
                    const auto r = helper::defer([&] {
                        s->decr();
                    });
                    auto app = reinterpret_cast<void (*)(void*, size_t, error::Error)>(s->appcb);
                    s->on_operation = false;
                    app(s->ctx, size, std::move(err));
                };
                set_error(0);
                socdl.WSASend_(suite->sock, &suite->plt.buf, 1, &suite->plt.read, suite->plt.flags, &suite->plt.ol, nullptr);
                return handleStart(rw, suite, [&] {
                    cb(fnctx, suite->plt.read, error::none);
                });
            });
        }

        error::Error Socket::writeto_async(view::rvec src, const NetAddrPort& naddr, void* fnctx, void (*cb)(void*, size_t, error::Error err), int flag) {
            return startIO(async_ctx, false, [&](RWAsyncSuite* rw, AsyncSuite* suite) -> std::pair<AsyncState, error::Error> {
                if (src.null()) {
                    return {AsyncState::failed, error::Error(invalid_argument, error::ErrorCategory::validationerr)};
                }
                if (auto err = winIOCommon(sock, src.data(), src.size(), suite, cb, fnctx, flag, false)) {
                    return {AsyncState::failed, err};
                }
                suite->cb = [](AsyncSuite* s, size_t size, error::Error err) {
                    const auto r = helper::defer([&] {
                        s->decr();
                    });
                    auto app = reinterpret_cast<void (*)(void*, size_t, error::Error)>(s->appcb);
                    s->on_operation = false;
                    app(s->ctx, size, std::move(err));
                };
                auto [addr, addrlen] = NetAddrPort_to_sockaddr(&suite->plt.addr, naddr);
                if (!addr) {
                    return {AsyncState::failed, error::Error(not_supported, error::ErrorCategory::dneterr)};
                }
                set_error(0);
                socdl.WSASendTo_(suite->sock, &suite->plt.buf, 1, &suite->plt.read, suite->plt.flags, addr, addrlen, &suite->plt.ol, nullptr);
                return handleStart(rw, suite, [&] {
                    cb(fnctx, suite->plt.read, error::none);
                });
            });
        }

        error::Error Socket::connect_async(const NetAddrPort& addr, void* fnctx, void (*cb)(void*, error::Error)) {
            return startIO(async_ctx, false, [&](RWAsyncSuite* rw, AsyncSuite* suite) -> std::pair<AsyncState, error::Error> {
                if (auto err = winIOCommon(sock, nullptr, 0, suite, cb, fnctx, 0, false)) {
                    return {AsyncState::failed, err};
                }
                suite->cb = [](AsyncSuite* s, size_t size, error::Error err) {
                    const auto r = helper::defer([&] {
                        s->decr();
                    });
                    auto app = reinterpret_cast<void (*)(void*, error::Error)>(s->appcb);
                    s->on_operation = false;
                    app(s->ctx, std::move(err));
                };
                sockaddr_storage st;
                auto [ptr, len] = NetAddrPort_to_sockaddr(&st, addr);
                if (!ptr) {
                    return {AsyncState::failed, error::Error(not_supported, error::ErrorCategory::dneterr)};
                }
                set_error(0);
                socdl.ConnectEx_(sock, ptr, len, nullptr, 0, nullptr, &suite->plt.ol);
                return handleStart(rw, suite, [&]() {
                    cb(fnctx, error::none);
                });
            });
        }

        error::Error Socket::accept_async(void* fnctx, void (*cb)(void*, Socket, NetAddrPort, error::Error)) {
            return error::unimplemented;  // now unimplemented

            return startIO(async_ctx, false, [&](RWAsyncSuite* rw, AsyncSuite* suite) -> std::pair<AsyncState, error::Error> {
                auto stlen = (sizeof(sockaddr_storage) + 16);
                if (auto err = winIOCommon(sock, nullptr, stlen * 2, suite, cb, fnctx, 0, false)) {
                    return {AsyncState::failed, err};
                }
                suite->cb = [](AsyncSuite* s, size_t, error::Error err) {

                };
                auto [af, type, proto, err] = get_af_type_protocol();
                if (err) {
                    return {AsyncState::failed, err};
                }
                suite->plt.accept_sock = socdl.WSASocketW_(af, type, proto, nullptr, 0, WSA_FLAG_OVERLAPPED);
                if (suite->plt.accept_sock == -1) {
                    return {AsyncState::failed, Errno()};
                }
                set_error(0);
                auto res = handleStart(rw, suite, [&] {

                });
                if (res.first == AsyncState::failed) {
                    sockclose(suite->plt.accept_sock);
                    suite->plt.accept_sock = ~0;
                }
                return res;
            });
        }
    }  // namespace dnet
}  // namespace utils
