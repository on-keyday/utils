/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "dll/dllh.h"
#include <cstdint>
#include <atomic>
#include "plthead.h"
#include "error.h"
#include <optional>
#include "address.h"
#include "socket.h"
#include "event/io.h"
#include <platform/detect.h>

namespace futils::fnet {
    struct SockTable;
    void destroy_platform(SockTable*);
    void sockclose(std::uintptr_t sock);
    Socket make_socket(void*);
    void set_nonblock(std::uintptr_t sock, bool yes = true);

    struct Socket;

    struct SockTable {
        std::uintptr_t sock = ~std::uintptr_t(0);
        event::IOEvent* event = nullptr;
        std::atomic_uint32_t refs;

        SockTable()
            : refs(1) {}

        void incr() {
            refs++;
        }

        std::uint32_t decr() {
            if (0 == --refs) {
                auto sock = this->sock;
                destroy_platform(this);
                sockclose(sock);
                return 0;
            }
            return refs;
        }
    };

    SockTable* make_sock_table(std::uintptr_t socket, event::IOEvent* event);

    enum class LockState {
        unlocked,
        locked,
        canceling,
    };

    struct CancelableLock {
       private:
        std::atomic<LockState> l;
        std::atomic_uint64_t current;

       public:
        /**
         * Attempts to acquire the lock.
         *
         * When executing try_lock:
         * - If the state is 'canceling': Another thread is performing a cancel operation. Nothing can be done.
         * - If the state is 'locked': Another thread is waiting. Nothing can be done.
         * - If the state is 'unlocked': The lock is acquired. However, there is a potential race condition between
         *   transitioning from 'locked' to '++current' and another thread transitioning to 'canceling'.
         *   To avoid this race, 'current' is incremented after unlocking.
         */
        expected<std::uint64_t> try_lock() {
            LockState expect = LockState::unlocked;
            if (!l.compare_exchange_strong(expect, LockState::locked, std::memory_order::acquire)) {
                return unexpect("cannot get lock", error::Category::lib, error::fnet_async_error);
            }
            auto code = ++current;
            return code;
        }

        /**
         * Attempts to interrupt an ongoing operation.
         *
         * When executing interrupt:
         * - If the state is 'canceling': Another thread is performing a cancel operation. Nothing can be done.
         * - If the state is 'locked': The state changes to 'canceling'.
         *   - Checks for consistency between 'code' and 'current'. If they do not match, the operation is rejected.
         *   - If they match, the cancel processing is executed.
         *   - On Windows, uses CancelIoEx; there is a potential race condition, but consistency is checked via current and code.
         *   - On Linux, the callback function is obtained atomically. A potential race condition exists,
         *     but it is also checked using current and code.
         *     There might be a competition between obtaining the callback and epoll_event,
         *     so atomic exchange is employed to ensure only one can acquire it.
         */
        expected<void> interrupt(std::uint64_t code, auto&& cancel) {
            LockState expect = LockState::locked;
            if (!l.compare_exchange_strong(expect, LockState::canceling, std::memory_order::acquire)) {
                return unexpect("cannot get lock", error::Category::lib, error::fnet_async_error);
            }
            auto d = helper::defer([&] {
                l.store(LockState::locked, std::memory_order::release);
            });
            if (current != code) {
                return unexpect("operation already done", error::Category::lib, error::fnet_async_error);
            }
            current++;
            return cancel();
        }

        /**
         * Releases the lock.
         *
         * When executing unlock:
         * - If the state is 'locked': Performs the unlock operation. A potential race condition exists
         *   with the next lock attempt. Following the one-socket-one-call rule can help mitigate this issue.
         * - If the state is 'unlocked': An invalid operation occurs.
         * - If the state is 'canceling': Wait until the cancel operation completes (it may fail, but still needs to wait).
         */
        expected<void> unlock() {
            LockState expect = LockState::locked;
            while (!l.compare_exchange_weak(expect, LockState::unlocked, std::memory_order::release)) {
                if (expect == LockState::unlocked) {
                    return unexpect("unlocking non-locked lock", error::Category::lib, error::fnet_async_error);
                }
                expect = LockState::locked;  // don't forget to reset expect
            }
            current++;
            return {};
        }
    };

    // dummy for type checking
    struct IOTableHeader {};

    struct NotifyCallback {
        void (*notify)() = nullptr;
        void* user = nullptr;
        using call_t = void (*)(void (*notify)(), void* user, IOTableHeader*, NotifyResult&&);
        std::atomic<call_t> call;

        void set_notify(void* u, auto f, call_t c) {
            user = u;
            notify = reinterpret_cast<void (*)()>(f);
            call = c;
        }
    };

#ifdef FUTILS_PLATFORM_WINDOWS

    struct WinSockTable;

    struct WinSockIOTableHeader : IOTableHeader {
        OVERLAPPED ol;
        WinSockTable* base;
        NotifyCallback cb;
        CancelableLock l;
        WSABUF bufs[1];
        // WSABUF* bufs;

        void set_buffer(view::rvec buf) {
            bufs[0].buf = (CHAR*)buf.as_char();
            bufs[0].len = buf.size();
            // bufs = &buf1;
        }
    };

    static_assert(offsetof(WinSockIOTableHeader, ol) == 0, "ol must be the first member of WinSockIOTableHeader");
    constexpr auto sizeof_sockaddr_storage = sizeof(sockaddr_storage);

    static_assert(sizeof_sockaddr_storage > sizeof(std::uintptr_t), "sockaddr_storage size is too small");

    struct WinSockReadTable : WinSockIOTableHeader {
        union {
            struct {
                byte storage[sizeof_sockaddr_storage - sizeof(std::uintptr_t)];
                std::uintptr_t accept_sock;
            };
            sockaddr_storage from;
        };
        int from_len;
    };

    constexpr auto acceptex_least_size_one = (std::max)(sizeof(sockaddr_in) + 17, sizeof(sockaddr_in6) + 17);
    constexpr auto acceptex_least_size = acceptex_least_size_one * 2;

    static_assert(sizeof_sockaddr_storage > acceptex_least_size + sizeof(std::uintptr_t), "sockaddr_storage size is too small");

    struct WinSockWriteTable : WinSockIOTableHeader {
        sockaddr_storage to;
    };

    struct WinSockTable : SockTable {
        WinSockReadTable r;
        WinSockWriteTable w;
        bool skip_notif = false;
    };

    constexpr auto sizeof_WinSockTable = sizeof(WinSockTable);
#elif defined(FUTILS_PLATFORM_LINUX)
    struct EpollTable;
    struct EpollIOTableHeader : IOTableHeader {
        NotifyCallback cb;
        CancelableLock l;
        EpollTable* base;
        std::atomic_bool epoll_lock;
    };

    struct EpollTable : SockTable {
        EpollIOTableHeader r;
        EpollIOTableHeader w;
    };

#endif

    void set_nonblock(std::uintptr_t sock, bool);
    void sockclose(std::uintptr_t sock);

    NetAddrPort sockaddr_to_NetAddrPort(sockaddr* addr, size_t len);

    std::pair<sockaddr*, int> NetAddrPort_to_sockaddr(sockaddr_storage* addr, const NetAddrPort&);
    expected<std::uintptr_t> socket_platform(SockAttr attr);
    expected<Socket> setup_socket(std::uintptr_t sock, event::IOEvent* event);

}  // namespace futils::fnet
