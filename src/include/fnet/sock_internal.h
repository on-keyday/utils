/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
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
        event::IOEvent* event;
        std::atomic_uint32_t refs;

        SockTable()
            : refs(1) {}

        void incr() {
            refs++;
        }

        void decr() {
            if (0 == --refs) {
                auto sock = this->sock;
                destroy_platform(this);
                sockclose(sock);
            }
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
        expected<std::uint64_t> try_lock() {
            LockState expect = LockState::unlocked;
            if (!l.compare_exchange_strong(expect, LockState::locked)) {
                return unexpect("cannot get lock", error::Category::lib, error::fnet_async_error);
            }
            auto code = ++current;
            return code;
        }

        expected<void> interrupt(std::uint64_t value, auto&& cancel) {
            LockState expect = LockState::locked;
            if (!l.compare_exchange_strong(expect, LockState::canceling)) {
                return unexpect("cannot get lock", error::Category::lib, error::fnet_async_error);
            }
            if (current != value) {
                return unexpect("operation already done", error::Category::lib, error::fnet_async_error);
            }
            current++;
            auto d = helper::defer([&] {
                l.store(LockState::locked);
            });
            return cancel();
        }

        expected<void> unlock() {
            LockState expect = LockState::locked;
            while (!l.compare_exchange_weak(expect, LockState::unlocked)) {
                if (expect == LockState::unlocked) {
                    return unexpect("unlocking non-locked lock", error::Category::lib, error::fnet_async_error);
                }
            }
            current++;
            return {};
        }
    };

    struct NotifyCallback {
        void (*notify)() = nullptr;
        void* user = nullptr;
        void (*call)(NotifyCallback*, void* base, NotifyResult&&) = nullptr;

        void set_notify(void* u, auto f, auto c) {
            user = u;
            notify = reinterpret_cast<void (*)()>(f);
            call = c;
        }
    };

#ifdef FUTILS_PLATFORM_WINDOWS

    struct WinSockTable;

    struct WinSockIOTableHeader {
        OVERLAPPED ol;
        WinSockTable* base;
        NotifyCallback cb;
        CancelableLock l;
        WSABUF buf1;
        WSABUF* bufs;

        void set_buffer(view::rvec buf) {
            buf1.buf = (CHAR*)buf.as_char();
            buf1.len = buf.size();
            bufs = &buf1;
        }
    };

    struct WinSockReadTable : WinSockIOTableHeader {
        sockaddr_storage from;
        int from_len;
    };

    struct WinSockWriteTable : WinSockIOTableHeader {
        sockaddr_storage to;
    };

    struct WinSockTable : SockTable {
        WinSockReadTable r;
        WinSockWriteTable w;
        bool skip_notif = false;
    };
#elif defined(FUTILS_PLATFORM_LINUX)
    struct EpollTable;
    struct EpollIOTableHeader {
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

}  // namespace futils::fnet
