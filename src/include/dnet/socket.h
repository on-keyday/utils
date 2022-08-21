/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// native_socket - native socket interface
#pragma once
#include <cstdint>
#include <cstddef>
#include <utility>
#include <memory>
#include "dll/dllh.h"

namespace utils {
    namespace dnet {

        constexpr auto async_reserved = 0xf93f4928;
        enum AsyncMethod {
            am_recv,
            am_recvfrom,
            am_accept,
            am_connect,
        };

        struct AsyncHead {
            const std::uint32_t reserved = async_reserved;
            // mode represents async operation method which is processing now
            AsyncMethod method;
#ifdef _WIN32
            using get_ptrs_t =
                void (*)(AsyncHead* head, void** overlapped, void** wsbuf, int* bufcount,
                         void** sockfrom, int** fromlen, void** flags, std::uintptr_t** tmpsock);
            // get_ptrs gets object pointers
            // this function is windows spec
            get_ptrs_t get_ptrs;
#endif
            // start notifys operation start
            // if you use object as shared_ptr
            // you should increment count here
            void (*start)(AsyncHead* head, std::uintptr_t sock);
            // completion handle io completion
            // on windows, len contains bytes received
            // on linux, len is events bits
            // you should do operation in short time
            // to prevent blocking at wait_event
            // if you use object as shared_ptr
            // you should decrement count here
            void (*completion)(AsyncHead* head, size_t len);
            // canceled is called when get_ptrs is called and io does not start
            // if you use object as shared_ptr
            // you should decrement count here
            void (*canceled)(AsyncHead* head);
        };

        // wait_event waits io completion until time passed
        // if event is processed function returns number of event
        // other wise returns 0
        dll_export(int) wait_event(std::uint32_t time);

        // Socket is wrappper class of native socket
        struct class_export Socket {
            ~Socket();
            constexpr Socket(Socket&& sock)
                : sock(std::exchange(sock.sock, ~0)),
                  err(std::exchange(sock.err, 0)),
                  opt(std::exchange(sock.opt, nullptr)),
                  gc_(std::exchange(sock.gc_, nullptr)),
                  prevred(std::exchange(sock.prevred, 0)) {}
            Socket& operator=(Socket&& sock) {
                if (this == &sock) {
                    return *this;
                }
                this->~Socket();
                this->sock = std::exchange(sock.sock, ~0);
                err = std::exchange(sock.err, 0);
                opt = std::exchange(sock.opt, nullptr);
                gc_ = std::exchange(sock.gc_, nullptr);
                prevred = std::exchange(sock.prevred, 0);
                return *this;
            }

            bool connect(const void* addr /* = sockaddr*/, size_t len);
            bool write(const void* data, size_t len, int flag = 0);
            bool read(void* data, size_t len, int flag = 0);
            bool writeto(const void* addr, int addrlen, const void* data, size_t len, int flag = 0);
            bool readfrom(void* addr, int* addrlen, void* data, size_t len, int flag = 0);
            [[nodiscard]] bool accept(Socket& to, void* addr, int* addrlen);
            using completion_accept_t = void (*)(void* user, Socket&& sock);
            bool accept_async(completion_accept_t completion, void* user, void* data = nullptr, size_t len = 0);
            using completion_connect_t = void (*)(void* user);
            bool connect_async(completion_connect_t completion, void* user, const void* addr, size_t len);
            bool bind(const void* addr /* = sockaddr*/, size_t len);
            bool listen(int back = 10);
            bool wait_readable(std::uint32_t sec, std::uint32_t usec);
            bool wait_writable(std::uint32_t sec, std::uint32_t usec);

            using completion_t = void (*)(void* user, void* data, size_t len, size_t bufmax);
            using completion_from_t = void (*)(void* user, void* data, size_t len, size_t bufmax, void* addr, size_t addrlen);
            // read_async reads socket async
            // if custom opt value is set
            // argument will be ignored
            bool read_async(completion_t completion, void* user, void* data = nullptr, size_t datalen = 0);
            // read_from_async reads socket async
            // if custom opt value is set
            // argument will be ignored
            bool readfrom_async(completion_from_t completion, void* user, void* data = nullptr, size_t datalen = 0);
            constexpr void set_custom_opt(void* optv, void (*gc)(void*, std::uintptr_t)) {
                if (gc_) {
                    gc_(opt, sock);
                }
                opt = optv;
                gc_ = gc;
            }
            constexpr void* get_opt() const {
                return opt;
            }
            constexpr explicit operator bool() const {
                return sock != ~0;
            }

            bool block() const;
            constexpr int geterr() const {
                return err;
            }
            bool shutdown(int mode = 2 /*= both*/);
            constexpr int readsize() const {
                return prevred;
            }

            template <class T>
            bool get_option(int layer, int opt, T& t) const {
                return get_option(layer, opt, std::addressof(t), sizeof(t));
            }

            template <class T>
            bool set_option(int layer, int opt, T&& t) {
                return set_option(layer, opt, std::addressof(t), sizeof(t));
            }

           private:
            std::uintptr_t sock;
            int err;
            int prevred;
            void* opt;
            void (*gc_)(void* opt, std::uintptr_t sock);

            constexpr Socket(std::uintptr_t s)
                : sock(s), prevred(0), opt(nullptr), gc_(nullptr) {}
            bool get_option(int layer, int opt, void* buf, size_t size);
            bool set_option(int layer, int opt, const void* buf, size_t size);
            friend Socket make_socket(std::uintptr_t uptr);

           public:
            constexpr Socket()
                : Socket(std::uintptr_t(~0)){};
        };

        // make_socket creates socket object
        // before this function calls
        // you mustn't call any other functions
        // if you call any function
        // that is undefined behaviour
        dll_export(Socket) make_socket(int address_family, int socket_type, int protocol);
    }  // namespace dnet
}  // namespace utils
