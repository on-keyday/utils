/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/dllcpp.h>
#include <dnet/socket.h>
#include <dnet/dll/sockdll.h>
#include <dnet/dll/glheap.h>
#include <dnet/errcode.h>
#ifdef _WIN32
#include <WS2tcpip.h>
#endif
#include <cstring>
#include <atomic>
#include <dnet/dll/sockasync.h>
namespace utils {
    namespace dnet {
#ifdef _WIN32
        void sockclose(std::uintptr_t sock) {
            socdl.closesocket_(sock);
        }

        void set_nonblock(std::uintptr_t sock) {
            u_long nb = 1;
            socdl.ioctlsocket_(sock, FIONBIO, &nb);
        }
#else
        void sockclose(std::uintptr_t sock) {
            socdl.close_(sock);
        }

        void set_nonblock(std::uintptr_t sock) {
            u_long nb = 1;
            socdl.ioctl_(sock, FIONBIO, &nb);
        }
#endif

        bool Socket::connect(const void* addr, size_t len) {
            auto res = socdl.connect_(sock, static_cast<const ::sockaddr*>(addr), len);
            if (res < 0) {
                err = get_error();
            }
            return res >= 0;
        }

        bool sock_wait(int& err, std::uintptr_t sock, std::uint32_t sec, std::uint32_t usec, bool read) {
            timeval val{};
            val.tv_sec = sec;
            val.tv_usec = usec;
            fd_set sets{}, xset{}, excset{};
            FD_ZERO(&sets);
            FD_SET(sock, &sets);
            memcpy(&xset, &sets, sizeof(fd_set));
            memcpy(&excset, &sets, sizeof(fd_set));
            int res = 0;
            if (read) {
                res = socdl.select_(sock + 1, &xset, nullptr, &excset, &val);
            }
            else {
                res = socdl.select_(sock + 1, nullptr, &xset, &excset, &val);
            }
            if (res == -1) {
                err = get_error();
                return false;
            }

#ifdef _WIN32
            if (socdl.__WSAFDIsSet_(sock, &excset)) {
                err = get_error();
                return false;
            }
            if (socdl.__WSAFDIsSet_(sock, &xset)) {
                return true;
            }
#else
            if (FD_ISSET(sock, &excset)) {
                err = get_error();
                return false;
            }
            if (FD_ISSET(sock, &xset)) {
                return true;
            }
#endif
            return false;
        }

        bool Socket::wait_readable(std::uint32_t sec, std::uint32_t usec) {
            return sock_wait(err, sock, sec, usec, true);
        }

        bool Socket::wait_writable(std::uint32_t sec, std::uint32_t usec) {
            return sock_wait(err, sock, sec, usec, false);
        }

        bool Socket::write(const void* data, size_t len, int flag) {
            auto res = socdl.send_(sock, static_cast<const char*>(data), len, flag);
            if (res < 0) {
                err = get_error();
            }
            return res >= 0;
        }

        bool Socket::read(void* data, size_t len, int flag) {
            auto res = socdl.recv_(sock, static_cast<char*>(data), len, flag);
            if (res < 0) {
                err = get_error();
            }
            else {
                prevred = res;
            }
            return res > 0;
        }

        bool Socket::writeto(const void* addr, int addrlen, const void* data, size_t len, int flag) {
            auto res = socdl.sendto_(sock, static_cast<const char*>(data), int(len), flag, static_cast<const sockaddr*>(addr), int(addrlen));
            if (res < 0) {
                err = get_error();
            }
            return res >= 0;
        }

        bool Socket::readfrom(void* addr, int* addrlen, void* data, size_t len, int flag) {
            if (!addrlen) {
                err = invalid_addr;
                return false;
            }
            socklen_t fromlen = *addrlen;
            auto res = socdl.recvfrom_(sock, static_cast<char*>(data), len, flag, static_cast<sockaddr*>(addr), &fromlen);
            if (res < 0) {
                err = get_error();
            }
            else {
                *addrlen = fromlen;
                prevred = res;
            }
            return res >= 0;
        }

        bool Socket::block() const {
#ifdef _WIN32
            return err == WSAEWOULDBLOCK;
#else
            return err == EINPROGRESS || err == EWOULDBLOCK;
#endif
        }

        bool Socket::shutdown(int mode) {
            auto res = socdl.shutdown_(sock, mode);
            if (res < 0) {
                err = get_error();
            }
            return res == 0;
        }

        bool Socket::get_option(int layer, int opt, void* buf, size_t size) {
            socklen_t len = int(size);
            auto res = socdl.getsockopt_(sock, layer, opt, static_cast<char*>(buf), &len);
            if (res < 0) {
                err = get_error();
            }
            return res == 0;
        }

        bool Socket::set_option(int layer, int opt, const void* buf, size_t size) {
            auto res = socdl.setsockopt_(sock, layer, opt, static_cast<const char*>(buf), int(size));
            if (res < 0) {
                err = get_error();
            }
            return res == 0;
        }

        bool Socket::set_reuse_addr(bool resue) {
            int yes = resue ? 1 : 0;
            return set_option(SOL_SOCKET, SO_REUSEADDR, yes);
        }

        bool Socket::set_ipv6only(bool only) {
            int yes = only ? 1 : 0;
            return set_option(IPPROTO_IPV6, IPV6_V6ONLY, yes);
        }

        bool Socket::bind(const void* addr, size_t len) {
            auto res = socdl.bind_(sock, static_cast<const sockaddr*>(addr), len);
            if (res < 0) {
                err = get_error();
            }
            return res == 0;
        }

        bool Socket::listen(int back) {
            auto res = socdl.listen_(sock, back);
            if (res < 0) {
                err = get_error();
            }
            return res == 0;
        }

        Socket make_socket(std::uintptr_t uptr) {
            return Socket(uptr);
        }

        bool Socket::accept(Socket& to, void* addr, int* addrlen) {
            if (to.sock != ~0) {
                err = non_invalid_socket;
                return false;
            }
            if (!addrlen) {
                err = invalid_addr;
                return false;
            }
            socklen_t len = *addrlen;
            auto new_sock = socdl.accept_(sock, static_cast<sockaddr*>(addr), &len);
            if (new_sock == -1) {
                err = get_error();
                return false;
            }
            *addrlen = len;
            set_nonblock(new_sock);
            to = make_socket(std::uintptr_t(new_sock));
            return true;
        }

        Socket::~Socket() {
            if (opt && gc_) {
                gc_(opt, sock);
            }
            if (sock != ~0) {
                sockclose(sock);
            }
        }

        bool init_sockdl() {
#ifdef _WIN32
            static auto result = socdl.load();
#else
            static auto result = socdl.load() && kerlib.load();
#endif
            return result;
        }

        dnet_dll_internal(Socket) make_socket(int address_family, int socket_mode, int protocol) {
            if (!init_sockdl()) {
                return make_socket(~0);
            }
#ifdef _WIN32
            auto sock = socdl.WSASocketW_(address_family, socket_mode, protocol, nullptr, 0, WSA_FLAG_OVERLAPPED);
#else
            auto sock = socdl.socket_(address_family, socket_mode, protocol);
#endif
            if (sock == -1) {
                return make_socket(~0);
            }
            set_nonblock(sock);
            return make_socket(sock);
        }

        bool initopt(void*& opt, void (*&gc)(void*, std::uintptr_t), int& err, void* user,
                     void* data, size_t datalen, completions_t comp, AsyncMethod mode) {
            if (!opt) {
                opt = AsyncBuffer_new();
                if (!opt) {
                    err = no_resource;
                    return false;
                }
                gc = AsyncBuffer_gc;
            }
            auto buf = static_cast<AsyncBufferCommon*>(opt);
            if (buf->reserved == defbuf_reserved) {
                if (buf->incomplete) {
                    err = operation_imcomplete;
                    return false;
                }
                buf->user = user;
                if (mode != am_connect) {
                    if (data && datalen) {
                        buf->ebuf.~EasyBuffer();
                        buf->ebuf.buf = (char*)data;
                        buf->ebuf.size = datalen;
                    }
                    else {
                        buf->ebuf = {2048};
                        if (!buf->ebuf.buf) {
                            err = no_resource;
                            return false;
                        }
                        buf->ebuf.should_del = true;
                    }
                }
                buf->comp = comp;
            }
            return true;
        }

        bool Socket::read_async(completion_t completion, void* user, void* data, size_t datalen) {
            if (!initopt(opt, gc_, err, user, data, datalen, completions_t(completion), am_recv)) {
                return false;
            }
            if (!start_async_operation(opt, sock, am_recv, nullptr, 0)) {
                err = get_error();
                return false;
            }
            return true;
        }

        bool Socket::readfrom_async(completion_from_t completion, void* user, void* data, size_t datalen) {
            if (!initopt(opt, gc_, err, user, data, datalen, completions_t(completion), am_recvfrom)) {
                return false;
            }
            if (!start_async_operation(opt, sock, am_recvfrom, nullptr, 0)) {
                err = get_error();
                return false;
            }
            return true;
        }

        bool Socket::accept_async(completion_accept_t completion, void* user, void* data, size_t datalen) {
            if (!initopt(opt, gc_, err, user, data, datalen, completions_t{completion}, am_accept)) {
                return false;
            }
            if (!start_async_operation(opt, sock, am_accept, nullptr, 0)) {
                err = get_error();
                return false;
            }
            return true;
        }

        bool Socket::connect_async(completion_connect_t completion, void* user, const void* addr, size_t len) {
            if (!initopt(opt, gc_, err, user, nullptr, 0, completions_t(completion), am_connect)) {
                return false;
            }
            if (!start_async_operation(opt, sock, am_connect, addr, len)) {
                err = get_error();
                return false;
            }
            return true;
        }

        dnet_dll_internal(int) wait_event(std::uint32_t time) {
            return wait_event_plt(time);
        }

        void* Socket::internal_alloc(size_t s) {
            return get_rawbuf(s);
        }

        void Socket::internal_free(void* p) {
            free_rawbuf(p);
        }

    }  // namespace dnet
}  // namespace utils
