/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// asyncbase - async file/network io base
#pragma once

#include "../plthead.h"
#include "dllh.h"
#include <cstdint>
#include <atomic>
#include "../storage.h"
#include "../../helper/defer.h"
#include "../address.h"
#include "../error.h"
#include "../../thread/waker.h"

namespace utils {
    namespace fnet {
#ifdef _WIN32
        struct PlatformAsyncSuite {
            OVERLAPPED ol{};
            WSABUF buf{};
            DWORD read;
            DWORD flags;
            sockaddr_storage addr;
            int addrlen;
            std::uintptr_t accept_sock = ~0;
        };

        struct PlatformRWAsyncSuite {
            bool skipNotif = false;
        };
#else
        struct PlatformAsyncSuite {
            std::atomic_bool lock;
            NetAddrPort addr;
            int flags;
            view::rvec buf;
        };

        struct PlatformRWAsyncSuite {
        };
#endif

        struct RWAsyncSuite;

        struct AsyncSuite {
            PlatformAsyncSuite plt;
            std::uintptr_t sock = ~0;
            void (*cb)(AsyncSuite* s, size_t size, error::Error err) = nullptr;
            std::shared_ptr<thread::Waker> waker;
            std::atomic_uint32_t storng_ref;
            std::atomic_bool on_operation = false;
            bool is_stream = false;

            void incr() {
                ++storng_ref;
            }

            void decr() {
                if (--storng_ref == 0) {
                    delete_glheap(this);
                }
            }

            AsyncSuite()
                : storng_ref(1) {}
        };

        struct RWAsyncSuite {
            PlatformRWAsyncSuite plt;
            AsyncSuite* r = nullptr;
            AsyncSuite* w = nullptr;
            std::atomic_uint32_t strong_ref = 0;

           private:
            AsyncSuite* init() {
                return new_from_global_heap<AsyncSuite>(DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(AsyncSuite), alignof(AsyncSuite)));
            }

           public:
            RWAsyncSuite()
                : strong_ref(1) {}

            void incr() {
                strong_ref++;
            }

            void decr() {
                if (--strong_ref == 0) {
                    delete_glheap(this);
                }
            }

            ~RWAsyncSuite() {
                if (r) {
                    r->decr();
                }
                if (w) {
                    w->decr();
                }
            }

            bool init_read() {
                if (!r) {
                    r = init();
                }
                return r != nullptr;
            }

            bool init_write() {
                if (!w) {
                    w = init();
                }
                return w != nullptr;
            }

            AsyncSuite* get_read() {
                if (r) {
                    r->incr();
                }
                return r;
            }

            AsyncSuite* get_write() {
                if (w) {
                    w->incr();
                }
                return w;
            }
        };

        inline RWAsyncSuite* getRWAsyncSuite(void*& p) {
            if (!p) {
                p = new_from_global_heap<RWAsyncSuite>(DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(RWAsyncSuite), alignof(RWAsyncSuite)));
            }
            return static_cast<RWAsyncSuite*>(p);
        }

        enum class AsyncState {
            started,
            completed,
            failed,
        };

        void remove_fd(std::uintptr_t fd);

        error::Error startIO(void*& p, bool read_mode, auto&& cb) {
            auto sp = getRWAsyncSuite(p);
            if (!sp) {
                return error::memory_exhausted;
            }
            auto& s = *sp;
            AsyncSuite* r;
            if (read_mode) {
                if (!s.init_read()) {
                    return error::memory_exhausted;
                }
                r = s.get_read();
            }
            else {
                if (!s.init_write()) {
                    return error::memory_exhausted;
                }
                r = s.get_write();
            }
            auto dec = helper::defer([&] {
                r->decr();
            });
            if (r->on_operation.exchange(true)) {
                return error::Error(read_mode
                                        ? "async read operation incompleted"
                                        : "async write operation incompleted",
                                    error::ErrorCategory::fneterr);
            }
            auto op = helper::defer([&] {
                r->on_operation = false;
            });
            auto [state, err] = cb(sp, r);
            if (state == AsyncState::started) {
                dec.cancel();
                op.cancel();
            }
            else if (state == AsyncState::completed) {
                op.cancel();
            }
            return err;
        }

        NetAddrPort sockaddr_to_NetAddrPort(sockaddr* addr, size_t len);

        std::pair<sockaddr*, int> NetAddrPort_to_sockaddr(sockaddr_storage* addr, const NetAddrPort&);

        int wait_event_plt(std::uint32_t time);

        void set_nonblock(std::uintptr_t sock, bool);
        void sockclose(std::uintptr_t sock);

        error::Error Errno();
    }  // namespace fnet
}  // namespace utils
