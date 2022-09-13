/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/sockasync.h>

#include <dnet/dll/sockdll.h>
#include <dnet/errcode.h>
#include <atomic>

namespace utils {
    namespace dnet {
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
                    delete_with_global_heap(this);
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
                        cmp.user_completion(user, ebuf.buf, res, ebuf.size);
                    }
                }
                else if (h->method == am_recvfrom) {
                    socklen_t len = sizeof(buf->storage);
                    res = socdl.recvfrom_(buf->sock, ebuf.buf, ebuf.size, 0, storage, &len);
                    if (res < 0) {
                        res = 0;  // ignore error
                    }
                    if (do_completion) {
                        cmp.user_completion_from(user, ebuf.buf, res, ebuf.size, &buf->storage, len);
                    }
                }
                else if (h->method == am_accept) {
                    socklen_t len = sizeof(buf->storage);
                    res = socdl.accept_(buf->sock, storage, &len);
                    set_nonblock(res);
                    if (do_completion) {
                        cmp.user_completion_accept(user, make_socket(std::uintptr_t(res)));
                    }
                }
                else if (h->method == am_connect) {
                    if (do_completion) {
                        cmp.user_completion_connect(user);
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
            return new_from_global_heap<AsyncBuffer>();
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
                auto h = async_head_from_context(ev->data.ptr);
                if (!h) {
                    continue;
                }
                h->completion(h, ev->events);
                proc++;
            }
            return proc;
        }

        bool start_async_operation(
            void* ptr, std::uintptr_t sock, AsyncMethod mode,
            /*for connectex*/
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
            return true;
        }

    }  // namespace dnet
}  // namespace utils
