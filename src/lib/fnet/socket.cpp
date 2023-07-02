/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/dll/dllcpp.h>
#include <fnet/dll/lazy/sockdll.h>
#include <fnet/socket.h>
#include <fnet/dll/glheap.h>
#include <cstring>
#include <atomic>
#include <fnet/dll/errno.h>
#include <fnet/dll/asyncbase.h>

namespace utils {
    namespace fnet {
#ifdef _WIN32
        void sockclose(std::uintptr_t sock) {
            lazy::closesocket_(sock);
        }

        void set_nonblock(std::uintptr_t sock, bool yes = true) {
            u_long nb = yes ? 1 : 0;
            lazy::ioctlsocket_(sock, FIONBIO, &nb);
        }
#else
        void sockclose(std::uintptr_t sock) {
            lazy::close_(sock);
        }

        void set_nonblock(std::uintptr_t sock, bool yes = true) {
            u_long nb = yes ? 1 : 0;
            lazy::ioctl_(sock, FIONBIO, &nb);
        }
#endif

        fnet_dll_implement(bool) isMsgSize(const error::Error& err) {
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

        fnet_dll_implement(bool) isSysBlock(const error::Error& err) {
            if (err == error::block) {
                return true;
            }
            if (err.category() != error::ErrorCategory::syserr) {
                return false;
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

        constexpr auto errAddrNotSupport = error::Error("NetAddrPort type not supported by fnet", error::ErrorCategory::fneterr);

        error::Error Socket::connect(const NetAddrPort& addr) {
            sockaddr_storage st{};
            auto [a, len] = NetAddrPort_to_sockaddr(&st, addr);
            if (!a) {
                return errAddrNotSupport;
            }
            auto res = lazy::connect_(sock, a, len);
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
                res = lazy::select_(sock + 1, &xset, nullptr, &excset, &val);
            }
            else {
                res = lazy::select_(sock + 1, nullptr, &xset, &excset, &val);
            }
            if (res == -1) {
                return Errno();
            }
#ifdef _WIN32
            if (lazy::__WSAFDIsSet_(sock, &excset)) {
                return Errno();
            }
            if (lazy::__WSAFDIsSet_(sock, &xset)) {
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

        std::pair<view::rvec, error::Error> Socket::write(view::rvec data, int flag) {
            auto sub = data.substr(0, (std::numeric_limits<socklen_t>::max)());
            auto res = lazy::send_(sock, sub.as_char(), sub.size(), flag);
            if (res < 0) {
                return {data, Errno()};
            }
            return {data.substr(res), error::none};
        }

        std::pair<view::wvec, error::Error> Socket::read(view::wvec data, int flag, bool is_stream) {
            auto sub = data.substr(0, (std::numeric_limits<socklen_t>::max)());
            auto res = lazy::recv_(sock, sub.as_char(), sub.size(), flag);
            if (res < 0) {
                return {{}, Errno()};
            }
            return {data.substr(0, res), res == 0 && is_stream ? error::eof : error::none};
        }

        std::pair<view::rvec, error::Error> Socket::writeto(const NetAddrPort& addr, view::rvec data, int flag) {
            sockaddr_storage st;
            auto [addrptr, addrlen] = NetAddrPort_to_sockaddr(&st, addr);
            if (!addrptr) {
                return {data, errAddrNotSupport};
            }
            auto sub = data.substr(0, (std::numeric_limits<socklen_t>::max)());
            auto res = lazy::sendto_(sock, sub.as_char(), sub.size(), flag, addrptr, addrlen);
            if (res < 0) {
                return {data, Errno()};
            }
            return {data.substr(res), error::none};
        }

        std::tuple<view::wvec, NetAddrPort, error::Error> Socket::readfrom(view::wvec data, int flag, bool is_stream) {
            sockaddr_storage soct{};
            socklen_t fromlen = sizeof(soct);
            auto addr = reinterpret_cast<sockaddr*>(&soct);
            auto sub = data.substr(0, (std::numeric_limits<socklen_t>::max)());
            auto res = lazy::recvfrom_(sock, sub.as_char(), sub.size(), flag, addr, &fromlen);
            if (res < 0) {
                return {{}, NetAddrPort{}, Errno()};
            }
            return {data.substr(0, res), sockaddr_to_NetAddrPort(addr, fromlen), res == 0 && is_stream ? error::eof : error::none};
        }

#ifdef _WIN32
        constexpr auto shutdown_recv = SD_RECEIVE;
        constexpr auto shutdown_send = SD_SEND;
        constexpr auto shutdown_both = SD_BOTH;
#else
        constexpr auto shutdown_recv = SHUT_RD;
        constexpr auto shutdown_send = SHUT_WR;
        constexpr auto shutdown_both = SHUT_RDWR;
#endif

        error::Error Socket::shutdown(ShutdownMode mode) {
            int mode_ = shutdown_both;
            switch (mode) {
                case ShutdownMode::send:
                    mode_ = shutdown_send;
                    break;
                case ShutdownMode::recv:
                    mode_ = shutdown_recv;
                    break;
                case ShutdownMode::both:  // nothing to do
                default:
                    break;
            }
            auto res = lazy::shutdown_(sock, mode_);
            if (res != 0) {
                return Errno();
            }
            return error::none;
        }

        Socket make_socket(std::uintptr_t uptr) {
            return Socket(uptr);
        }

        error::Error Socket::bind(const NetAddrPort& addr) {
            ::sockaddr_storage storage;
            auto [addr_ptr, len] = NetAddrPort_to_sockaddr(&storage, addr);
            if (!addr_ptr) {
                return errAddrNotSupport;
            }
            auto res = lazy::bind_(sock, addr_ptr, len);
            if (res != 0) {
                return Errno();
            }
            return error::none;
        }

        error::Error Socket::listen(int back) {
            auto res = lazy::listen_(sock, back);
            if (res != 0) {
                return Errno();
            }
            return error::none;
        }

        std::tuple<Socket, NetAddrPort, error::Error> Socket::accept() {
            sockaddr_storage st{};
            socklen_t addrlen = sizeof(st);
            auto ptr = reinterpret_cast<sockaddr*>(&st);
            auto new_sock = lazy::accept_(sock, ptr, &addrlen);
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

        fnet_dll_implement(Socket) make_socket(SockAttr attr) {
            if (!lazy::load_socket()) {
                return make_socket(~0);
            }
#ifdef _WIN32
            auto sock = lazy::WSASocketW_(attr.address_family, attr.socket_type, attr.protocol, nullptr, 0, WSA_FLAG_OVERLAPPED);
#else
            auto sock = lazy::socket_(attr.address_family, attr.socket_type, attr.protocol);
#endif
            if (sock == -1) {
                return make_socket(~0);
            }
            set_nonblock(sock);
            return make_socket(sock);
        }

        fnet_dll_implement(int) wait_io_event(std::uint32_t time) {
            return wait_event_plt(time);
        }
    }  // namespace fnet
}  // namespace utils
