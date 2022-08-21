/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "glheap.h"
#include "../socket.h"
#ifdef _WIN32
#include <windows.h>
#endif
namespace utils {
    namespace dnet {
        constexpr auto defbuf_reserved = 0x994827f3;

        struct EasyBuffer {
            char* buf;
            size_t size;
            bool should_del = false;
            constexpr EasyBuffer() {}

            [[nodiscard]] EasyBuffer(size_t sz) {
                buf = get_cvec(sz);
                size = sz;
            }

            ~EasyBuffer() {
                if (should_del) {
                    free_cvec(buf);
                }
                buf = nullptr;
            }
        };

        union completions_t {
            Socket::completion_t user_completion;
            Socket::completion_from_t user_completion_from;
            Socket::completion_accept_t user_completion_accept;
            Socket::completion_connect_t user_completion_connect;
            constexpr completions_t()
                : user_completion(nullptr) {}
            constexpr completions_t(Socket::completion_t c)
                : user_completion(c) {}
            constexpr completions_t(Socket::completion_from_t c)
                : user_completion_from(c) {}
            constexpr completions_t(Socket::completion_accept_t c)
                : user_completion_accept(c) {}
            constexpr completions_t(Socket::completion_connect_t c)
                : user_completion_connect(c) {}
        };

        bool start_async_operation(
            void* ptr, std::uintptr_t sock, AsyncMethod mode,
            /*for connectex*/
            const void* addr, size_t addrlen);

        struct AsyncBufferCommon {
            AsyncHead head;
#ifdef _WIN32
            OVERLAPPED ol{};
#endif
            const std::uint32_t reserved = defbuf_reserved;
            EasyBuffer ebuf;
            completions_t comp{};
            bool incomplete = false;
            void* user = nullptr;
        };

        inline AsyncHead* async_head_from_context(void* ol) {
            if (!ol) {
                return nullptr;
            }
            void* h = static_cast<char*>(ol)
#ifdef _WIN32
                      - sizeof(AsyncHead)
#endif
                ;
            auto t = static_cast<AsyncHead*>(h);
            if (t->reserved == async_reserved) {
                return t;
            }
            return nullptr;
        }

        struct AsyncBuffer;
        AsyncBuffer* AsyncBuffer_new();

        void AsyncBuffer_gc(void*, std::uintptr_t);

        int wait_event_plt(std::uint32_t time);

        void set_nonblock(std::uintptr_t sock);
        void sockclose(std::uintptr_t sock);

    }  // namespace dnet
}  // namespace utils
