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
                {
                    const auto r = helper::defer([&] {
                        async_head->decr();
                    });
                    async_head->cb(async_head, ent[i].dwNumberOfBytesTransferred, std::move(err));
                }
                ev++;
            }
            return ev;
        }

        error::Error winIOCommon(std::uintptr_t sock, const byte* data, size_t len, AsyncSuite* suite, std::shared_ptr<thread::Waker>& w, int flag, bool is_stream) {
            if (!register_handle(sock)) {
                return Errno();
            }
            suite->sock = sock;
            suite->plt.buf.buf = reinterpret_cast<CHAR*>(const_cast<byte*>(data));
            suite->plt.buf.len = len;
            suite->plt.ol = {};
            suite->plt.addrlen = sizeof(suite->plt.addr);
            suite->is_stream = is_stream;
            suite->plt.flags = flag;
            suite->waker = std::move(w);
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

        error::Error Socket::read_async(view::wvec buf, std::shared_ptr<thread::Waker> waker, int flag, bool is_stream) {
            if (buf.null()) {
                return error::Error("buf MUST NOT be null", error::ErrorCategory::validationerr);
            }
            return startIO(async_ctx, true, [&](RWAsyncSuite* rw, AsyncSuite* suite) -> std::pair<AsyncState, error::Error> {
                if (auto err = winIOCommon(sock, buf.data(), buf.size(), suite, waker, flag, is_stream)) {
                    return {AsyncState::failed, err};
                }
                suite->cb = [](AsyncSuite* s, size_t size, error::Error err) {
                    internal::WakerArg arg;
                    arg.size = size;
                    arg.err = std::move(err);
                    auto w = std::move(s->waker);
                    s->on_operation = false;
                    w->wakeup(&arg);
                };
                set_error(0);
                socdl.WSARecv_(suite->sock, &suite->plt.buf, 1, &suite->plt.read, &suite->plt.flags, &suite->plt.ol, nullptr);
                return handleStart(rw, suite, [&] {
                    internal::WakerArg arg;
                    arg.size = suite->plt.read;
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
                if (auto err = winIOCommon(sock, buf.data(), buf.size(), suite, waker, flag, is_stream)) {
                    return {AsyncState::failed, err};
                }
                suite->cb = [](AsyncSuite* s, size_t size, error::Error err) {
                    internal::WakerArg arg;
                    arg.size = size;
                    arg.err = std::move(err);
                    arg.addr = sockaddr_to_NetAddrPort(reinterpret_cast<sockaddr*>(&s->plt.addr), s->plt.addrlen);
                    auto w = std::move(s->waker);
                    s->on_operation = false;
                    w->wakeup(&arg);
                };
                auto addr = reinterpret_cast<sockaddr*>(&suite->plt.addr);
                set_error(0);
                socdl.WSARecvFrom_(suite->sock, &suite->plt.buf, 1, &suite->plt.read, &suite->plt.flags,
                                   addr, &suite->plt.addrlen, &suite->plt.ol, nullptr);
                return handleStart(rw, suite, [&] {
                    internal::WakerArg arg;
                    arg.size = suite->plt.read;
                    arg.addr = sockaddr_to_NetAddrPort(reinterpret_cast<sockaddr*>(&suite->plt.addr), suite->plt.addrlen);
                    auto w = std::move(suite->waker);
                    w->wakeup(&arg);
                });
            });
        }

        error::Error Socket::write_async(view::rvec src, std::shared_ptr<thread::Waker> waker, int flag) {
            if (src.null()) {
                return {error::Error("buf MUST NOT be null", error::ErrorCategory::validationerr)};
            }
            return startIO(async_ctx, false, [&](RWAsyncSuite* rw, AsyncSuite* suite) -> std::pair<AsyncState, error::Error> {
                if (auto err = winIOCommon(sock, src.data(), src.size(), suite, waker, flag, false)) {
                    return {AsyncState::failed, err};
                }
                suite->cb = [](AsyncSuite* s, size_t size, error::Error err) {
                    internal::WakerArg arg;
                    arg.size = size;
                    arg.err = std::move(err);
                    auto w = std::move(s->waker);
                    s->on_operation = false;
                    w->wakeup(&arg);
                };
                set_error(0);
                socdl.WSASend_(suite->sock, &suite->plt.buf, 1, &suite->plt.read, suite->plt.flags, &suite->plt.ol, nullptr);
                return handleStart(rw, suite, [&] {
                    internal::WakerArg arg;
                    arg.size = suite->plt.read;
                    auto w = std::move(suite->waker);
                    w->wakeup(&arg);
                });
            });
        }

        error::Error Socket::writeto_async(view::rvec src, const NetAddrPort& naddr, std::shared_ptr<thread::Waker> waker, int flag) {
            return startIO(async_ctx, false, [&](RWAsyncSuite* rw, AsyncSuite* suite) -> std::pair<AsyncState, error::Error> {
                if (src.null()) {
                    return {AsyncState::failed, error::Error(invalid_argument, error::ErrorCategory::validationerr)};
                }
                if (auto err = winIOCommon(sock, src.data(), src.size(), suite, waker, flag, false)) {
                    return {AsyncState::failed, err};
                }
                suite->cb = [](AsyncSuite* s, size_t size, error::Error err) {
                    internal::WakerArg arg;
                    arg.size = size;
                    arg.err = std::move(err);
                    auto w = std::move(s->waker);
                    s->on_operation = false;
                    w->wakeup(&arg);
                };
                auto [addr, addrlen] = NetAddrPort_to_sockaddr(&suite->plt.addr, naddr);
                if (!addr) {
                    return {AsyncState::failed, error::Error(not_supported, error::ErrorCategory::dneterr)};
                }
                set_error(0);
                socdl.WSASendTo_(suite->sock, &suite->plt.buf, 1, &suite->plt.read, suite->plt.flags, addr, addrlen, &suite->plt.ol, nullptr);
                return handleStart(rw, suite, [&] {
                    internal::WakerArg arg;
                    arg.size = suite->plt.read;
                    auto w = std::move(suite->waker);
                    w->wakeup(&arg);
                });
            });
        }

        error::Error Socket::connect_async(const NetAddrPort& addr, std::shared_ptr<thread::Waker> waker) {
            return startIO(async_ctx, false, [&](RWAsyncSuite* rw, AsyncSuite* suite) -> std::pair<AsyncState, error::Error> {
                if (auto err = winIOCommon(sock, nullptr, 0, suite, waker, 0, false)) {
                    return {AsyncState::failed, err};
                }
                suite->cb = [](AsyncSuite* s, size_t size, error::Error err) {
                    internal::WakerArg arg;
                    arg.size = size;
                    arg.err = std::move(err);
                    auto w = std::move(s->waker);
                    s->on_operation = false;
                    w->wakeup(&arg);
                };
                sockaddr_storage st;
                auto [ptr, len] = NetAddrPort_to_sockaddr(&st, addr);
                if (!ptr) {
                    return {AsyncState::failed, error::Error(not_supported, error::ErrorCategory::dneterr)};
                }
                set_error(0);
                socdl.ConnectEx_(sock, ptr, len, nullptr, 0, nullptr, &suite->plt.ol);
                return handleStart(rw, suite, [&]() {
                    internal::WakerArg arg;
                    auto w = std::move(suite->waker);
                    w->wakeup(&arg);
                });
            });
        }

        error::Error Socket::accept_async(std::shared_ptr<thread::Waker> waker) {
            return error::unimplemented;  // now unimplemented

            return startIO(async_ctx, false, [&](RWAsyncSuite* rw, AsyncSuite* suite) -> std::pair<AsyncState, error::Error> {
                auto stlen = (sizeof(sockaddr_storage) + 16);
                if (auto err = winIOCommon(sock, nullptr, stlen * 2, suite, waker, 0, false)) {
                    return {AsyncState::failed, err};
                }
                suite->cb = [](AsyncSuite* s, size_t, error::Error err) {

                };
                auto [attr, err] = get_sockattr();
                if (err) {
                    return {AsyncState::failed, err};
                }
                suite->plt.accept_sock = socdl.WSASocketW_(attr.address_family, attr.socket_type, attr.protocol, nullptr, 0, WSA_FLAG_OVERLAPPED);
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

        bool Socket::cancel_io(bool is_read) {
            auto ctx = getRWAsyncSuite(async_ctx);
            AsyncSuite* s = nullptr;
            if (is_read) {
                s = ctx->get_read();
            }
            else {
                s = ctx->get_write();
            }
            if (!s || !s->on_operation) {
                return false;
            }
            if (!kerlib.CancelIoEx_(HANDLE(this->sock), &s->plt.ol)) {
                if (get_error() != ERROR_NOT_FOUND) {
                    abort();  // no way to continue
                }
            }
            return true;
        }
    }  // namespace dnet
}  // namespace utils
