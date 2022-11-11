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
#include <cstring>
#include <atomic>
#include <dnet/dll/errno.h>
#include <dnet/dll/asyncbase.h>

namespace utils {
    namespace dnet {
#ifdef _WIN32
        void sockclose(std::uintptr_t sock) {
            socdl.closesocket_(sock);
        }

        void set_nonblock(std::uintptr_t sock, bool yes = true) {
            u_long nb = yes ? 1 : 0;
            socdl.ioctlsocket_(sock, FIONBIO, &nb);
        }
#else
        void sockclose(std::uintptr_t sock) {
            socdl.close_(sock);
        }

        void set_nonblock(std::uintptr_t sock, bool yes = true) {
            u_long nb = yes ? 1 : 0;
            socdl.ioctl_(sock, FIONBIO, &nb);
        }
#endif

        dnet_dll_implement(bool) isMsgSize(const error::Error& err) {
            if (err.category() != error::ErrorCategory::syserr) {
                return false;
            }
            if (err.type() != error::ErrorType::number) {
                return false;
            }
#ifdef _WIN32
            return err.errnum() == WSAEMSGSIZE;
#else
            return err.errnum() == EMSGSIZE;
#endif
        }

        dnet_dll_implement(bool) isBlock(const error::Error& err) {
            if (err.category() != error::ErrorCategory::syserr) {
                return false;
            }
            if (err == error::block) {
                return true;
            }
            if (err.type() != error::ErrorType::number) {
                return false;
            }
#ifdef _WIN32
            return err.errnum() == WSAEWOULDBLOCK;
#else
            return err.errnum() == EINPROGRESS || err.errnum() == EWOULDBLOCK;
#endif
        }

        error::Error Errno() {
            return error::Error(get_error(), error::ErrorCategory::syserr);
        }

        error::Error Socket::connect(const void* addr, size_t len) {
            auto res = socdl.connect_(sock, static_cast<const ::sockaddr*>(addr), len);
            if (res < 0) {
                return Errno();
            }
            return error::none;
        }

        error::Error sock_wait(std::uintptr_t sock, std::uint32_t sec, std::uint32_t usec, bool read) {
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
                return Errno();
            }
#ifdef _WIN32
            if (socdl.__WSAFDIsSet_(sock, &excset)) {
                return Errno();
            }
            if (socdl.__WSAFDIsSet_(sock, &xset)) {
                return error::none;
            }
#else
            if (FD_ISSET(sock, &excset)) {
                return Errno();
            }
            if (FD_ISSET(sock, &xset)) {
                return error::none;
            }
#endif
            return error::block;
        }

        error::Error Socket::wait_readable(std::uint32_t sec, std::uint32_t usec) {
            return sock_wait(sock, sec, usec, true);
        }

        error::Error Socket::wait_writable(std::uint32_t sec, std::uint32_t usec) {
            return sock_wait(sock, sec, usec, false);
        }

        std::pair<size_t, error::Error> Socket::write(const void* data, size_t len, int flag) {
            auto res = socdl.send_(sock, static_cast<const char*>(data), len, flag);
            if (res < 0) {
                return {0, Errno()};
            }
            return {res, error::none};
        }

        std::pair<size_t, error::Error> Socket::read(void* data, size_t len, int flag) {
            auto res = socdl.recv_(sock, static_cast<char*>(data), len, flag);
            if (res < 0) {
                return {0, Errno()};
            }
            return {res, res == 0 ? error::eof : error::none};
        }

        std::pair<size_t, error::Error> Socket::writeto(const raw_address* addr, int addrlen, const void* data, size_t len, int flag) {
            auto res = socdl.sendto_(sock, static_cast<const char*>(data), int(len), flag, reinterpret_cast<const sockaddr*>(addr), int(addrlen));
            if (res < 0) {
                return {0, Errno()};
            }
            return {res, error::none};
        }

        std::pair<size_t, error::Error> Socket::writeto(const NetAddrPort& addr, const void* data, size_t len, int flag) {
            sockaddr_storage st;
            auto [addrptr, addrlen] = NetAddrPort_to_sockaddr(&st, addr);
            if (!addrptr) {
                return {0, error::Error(not_supported, error::ErrorCategory::validationerr)};
            }
            auto res = socdl.sendto_(sock, static_cast<const char*>(data), len, flag, addrptr, addrlen);
            if (res < 0) {
                return {0, Errno()};
            }
            return {res, error::none};
        }

        std::pair<size_t, error::Error> Socket::readfrom(raw_address* addr, int* addrlen, void* data, size_t len, int flag) {
            if (!addrlen) {
                return {0, error::Error(invalid_addr, error::ErrorCategory::validationerr)};
            }
            socklen_t fromlen = *addrlen;
            auto res = socdl.recvfrom_(sock, static_cast<char*>(data), len, flag, reinterpret_cast<sockaddr*>(addr), &fromlen);
            if (res < 0) {
                return {0, error::Error(get_error(), error::ErrorCategory::syserr)};
            }
            *addrlen = fromlen;
            return {res, error::none};
        }

        std::tuple<size_t, NetAddrPort, error::Error> Socket::readfrom(void* data, size_t len, int flag) {
            sockaddr_storage soct{};
            socklen_t fromlen = sizeof(soct);
            auto addr = reinterpret_cast<sockaddr*>(&soct);
            auto res = socdl.recvfrom_(sock, static_cast<char*>(data), len, flag, addr, &fromlen);
            if (res < 0) {
                return {0, NetAddrPort{}, Errno()};
            }
            return {res, sockaddr_to_NetAddrPort(addr, fromlen), error::none};
        }

        error::Error Socket::shutdown(int mode) {
            auto res = socdl.shutdown_(sock, mode);
            if (res != 0) {
                return Errno();
            }
            return error::none;
        }

        Socket make_socket(std::uintptr_t uptr) {
            return Socket(uptr);
        }

        error::Error Socket::bind(const raw_address* addr, size_t len) {
            auto res = socdl.bind_(sock, reinterpret_cast<const sockaddr*>(addr), len);
            if (res != 0) {
                return Errno();
            }
            return error::none;
        }

        error::Error Socket::listen(int back) {
            auto res = socdl.listen_(sock, back);
            if (res != 0) {
                return Errno();
            }
            return error::none;
        }

        error::Error Socket::accept(Socket& to, raw_address* addr, int* addrlen) {
            if (to.sock != ~0) {
                return error::Error(non_invalid_socket, error::ErrorCategory::validationerr);
            }
            if (!addrlen) {
                return error::Error(invalid_addr, error::ErrorCategory::validationerr);
            }
            socklen_t len = *addrlen;
            auto new_sock = socdl.accept_(sock, reinterpret_cast<sockaddr*>(addr), &len);
            if (new_sock == -1) {
                return Errno();
            }
            *addrlen = len;
            set_nonblock(new_sock);
            to = make_socket(std::uintptr_t(new_sock));
            return error::none;
        }

        std::tuple<Socket, NetAddrPort, error::Error> Socket::accept() {
            sockaddr_storage st{};
            socklen_t addrlen = sizeof(st);
            auto ptr = reinterpret_cast<sockaddr*>(&st);
            auto new_sock = socdl.accept_(sock, ptr, &addrlen);
            if (new_sock == -1) {
                return {Socket(), NetAddrPort(), Errno()};
            }
            set_nonblock(new_sock);
            return {
                make_socket(std::uintptr_t(new_sock)),
                sockaddr_to_NetAddrPort(ptr, addrlen),
                error::none,
            };
        }

        Socket::~Socket() {
            if (async_ctx) {
                remove_fd(sock);
                auto p = static_cast<RWAsyncSuite*>(async_ctx);
                p->decr();
            }
            if (sock != ~0) {
                sockclose(sock);
            }
        }

        bool init_sockdl() {
            static auto result = socdl.load() && kerlib.load();
            return result;
        }

        dnet_dll_implement(Socket) make_socket(int address_family, int socket_mode, int protocol) {
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

        dnet_dll_implement(int) wait_event(std::uint32_t time) {
            return wait_event_plt(time);
        }
    }  // namespace dnet
}  // namespace utils
