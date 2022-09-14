/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/dllcpp.h>
#include <dnet/dll/sockdll.h>
#include <dnet/dll/sockasync.h>
#include <dnet/errcode.h>

namespace utils {
    namespace dnet {
        Socket make_socket(uintptr_t uptr);
        struct AsyncBuffer {
            AsyncBufferCommon common;
            WSABUF buf;
            ::sockaddr_storage storage;
            int stlen = 0;
            DWORD flags = 0;

            std::uintptr_t tmpsock = ~0;  // for accept mode
            std::atomic_uint32_t ref;
            void incref() {
                ref++;
            }

            void decref() {
                if (ref.fetch_sub(1) == 1) {
                    delete_with_global_heap(this);
                }
            }

            static void get_ptrs(AsyncHead* h, void** ol, void** wsabuf, int* bufcount, void** storage,
                                 int** stlen, void** flags, std::uintptr_t** tmpsockp) {
                auto buf = reinterpret_cast<AsyncBuffer*>(h);
                *ol = &buf->common.ol;
                buf->buf.buf = buf->common.ebuf.buf;
                buf->buf.len = buf->common.ebuf.size;
                *wsabuf = &buf->buf;
                *bufcount = 1;
                *storage = &buf->storage;
                buf->stlen = sizeof(buf->storage);
                *stlen = &buf->stlen;
                *flags = &buf->flags;
                *tmpsockp = &buf->tmpsock;
            }

            static void start(AsyncHead* h, std::uintptr_t) {
                auto buf = reinterpret_cast<AsyncBuffer*>(h);
                buf->incref();
            }
            static void completion(AsyncHead* h, size_t len) {
                if (!h) {
                    return;
                }
                auto buf = reinterpret_cast<AsyncBuffer*>(h);
                auto& comp = buf->common.comp;
                if (comp.user_completion) {
                    auto mode = buf->common.head.method;
                    auto user = buf->common.user;
                    auto& ebuf = buf->common.ebuf;
                    if (mode == am_recvfrom) {
                        comp.user_completion_from(user, ebuf.buf, len,
                                                  ebuf.size, &buf->storage, buf->stlen);
                    }
                    else if (mode == am_recv) {
                        comp.user_completion(user, ebuf.buf, len, ebuf.size);
                    }
                    else if (mode == am_accept) {
                        comp.user_completion_accept(user, make_socket(buf->tmpsock));
                    }
                    else if (mode == am_connect) {
                        comp.user_completion_connect(user);
                    }
                }
                buf->decref();
            }

            static void canceled(AsyncHead* h) {
                auto buf = reinterpret_cast<AsyncBuffer*>(h);
                buf->decref();
            }

            constexpr AsyncBuffer()
                : ref(1) {
                common.head.get_ptrs = get_ptrs;
                common.head.start = start;
                common.head.completion = completion;
                common.head.canceled = canceled;
            }
        };

        AsyncBuffer* AsyncBuffer_new() {
            return new_from_global_heap<AsyncBuffer>();
        }

        void AsyncBuffer_gc(void* ptr, std::uintptr_t sock) {
            auto buf = static_cast<AsyncBuffer*>(ptr);
            if (buf->common.incomplete) {
                kerlib.CancelIoEx_((HANDLE)sock, nullptr);
                SleepEx(0, true);
            }
            buf->decref();
        }

        struct Needs {
            OVERLAPPED* ol;
            WSABUF* buf = nullptr;
            sockaddr_storage* storage = nullptr;
            int* stlen = 0;
            DWORD* flags = nullptr;
            int bufcount = 0;
            std::uintptr_t* tmpsock;
        };

        void get_ptrs(AsyncHead* head, Needs& needs) {
            head->get_ptrs(head, (void**)&needs.ol, (void**)&needs.buf, &needs.bufcount,
                           (void**)&needs.storage, &needs.stlen, (void**)&needs.flags, &needs.tmpsock);
        }

        bool validate(AsyncHead* head, Needs& needs, AsyncMethod mode) {
            const auto olptr = (const char*)(head) + sizeof(AsyncHead);
            if (!needs.ol || !needs.buf || needs.bufcount <= 0 || !needs.flags) {
                set_error(invalid_get_ptrs_result);
                return false;
            }
            if (mode == AsyncMethod::am_recvfrom) {
                if (!needs.stlen || *needs.stlen <= 0 || !needs.storage) {
                    set_error(invalid_get_ptrs_result);
                    return false;
                }
            }
            if (mode == AsyncMethod::am_accept) {
                if (!needs.tmpsock) {
                    set_error(invalid_get_ptrs_result);
                    return false;
                }
            }
            if ((void*)(needs.ol) != (void*)(olptr)) {
                set_error(invalid_ol_ptr_position);
                return false;
            }
            return true;
        }

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
            return kerlib.CreateIoCompletionPort_((HANDLE)handle, iocp, 0, 0) != nullptr;
        }

        static bool start_async_operation(
            void* ptr, std::uintptr_t sock, AsyncMethod mode,
            /*for connectex*/
            const void* addr = nullptr, size_t addrlen = 0) {
            if (!register_handle(sock)) {
                if (sock == ~0) {
                    return false;  // yes completely wrong
                }
                // TODO(on-keyday):
                // multiple registration is failed with ERROR_INVALID_PARAMETER
                // should we hold flag of registration?
                if (get_error() != ERROR_INVALID_PARAMETER) {
                    return false;
                }
            }
            AsyncHead* head = static_cast<AsyncHead*>(ptr);
            if (head->reserved != async_reserved || !head->get_ptrs || !head->completion) {
                set_error(invalid_async_head);
                return false;
            }
            head->method = mode;
            if (head->start) {
                head->start(head, sock);
            }
            Needs needs;
            get_ptrs(head, needs);
            if (!validate(head, needs, mode)) {
                if (head->canceled) {
                    // if canceld
                    head->canceled(head);
                }
                return false;
            }
            int res = -1;
            DWORD trsf = 0;
            if (mode == am_recvfrom) {
                res = socdl.WSARecvFrom_(sock, needs.buf, needs.bufcount, &trsf, needs.flags,
                                         reinterpret_cast<sockaddr*>(needs.storage), needs.stlen, needs.ol, nullptr);
            }
            else if (mode == am_recv) {
                res = socdl.WSARecv_(sock, needs.buf, needs.bufcount, &trsf, needs.flags, needs.ol, nullptr);
            }
            else if (mode == am_accept) {
                // TODO(on-keyday): support other protocol?
                auto len = needs.buf->len;
                auto reqlen = sizeof(sockaddr_in6) + 16;
                auto tmp = socdl.WSASocketW_(AF_INET6, SOCK_STREAM, 0, nullptr, 0, WSA_FLAG_OVERLAPPED);
                if (tmp != -1) {
                    goto ENDACCEPT;
                }
                set_nonblock(tmp);
                if (len < reqlen * 2) {
                    set_error(invalid_get_ptrs_result);
                    sockclose(tmp);
                    goto ENDACCEPT;
                }
                *needs.tmpsock = tmp;
                if (socdl.AcceptEx_(sock, tmp, needs.buf->buf, len - reqlen * 2, reqlen, reqlen, needs.flags, needs.ol)) {
                    // syncronized completion
                    head->completion(head, *needs.flags);
                    res = 0;
                }
            ENDACCEPT:;
            }
            else if (mode == am_connect) {
                if (socdl.ConnectEx_(sock, static_cast<const sockaddr*>(addr), addrlen, nullptr, 0, nullptr, needs.ol)) {
                    // syncronized completion
                    head->completion(head, 0);
                    res = 0;
                }
            }
            else {
                set_error(not_supported);
            }
            if (res != 0) {
                res = get_error();
                if (res != WSA_IO_PENDING) {
                    if (head->canceled) {
                        head->canceled(head);
                    }
                    return false;
                }
            }
            return true;
        }

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
                auto async_head = async_head_from_context(ent[i].lpOverlapped);
                if (!async_head) {
                    continue;
                }
                async_head->completion(async_head, ent[i].dwNumberOfBytesTransferred);
                ev++;
            }
            return ev;
        }
    }  // namespace dnet
}  // namespace utils
