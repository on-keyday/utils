/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/dll/dllcpp.h>
#include <fnet/dll/lazy/sockdll.h>
#include <fnet/dll/lazy/load_error.h>
#include <fnet/socket.h>
#include <fnet/dll/glheap.h>
#include <cstring>
#include <atomic>
#include <fnet/dll/errno.h>
#include <fnet/sock_internal.h>
#include <fnet/event/io.h>
#include <platform/detect.h>

namespace futils {
    namespace fnet {
        void decr_table(void*);

        fnet_dll_implement(bool) isMsgSize(const error::Error& err) {
            if (err.category() != error::Category::os) {
                return false;
            }
            if (err.type() != error::ErrorType::number) {
                return false;
            }
#ifdef FUTILS_PLATFORM_WINDOWS
            return err.code() == WSAEMSGSIZE;
#else
            return err.code() == EMSGSIZE;
#endif
        }

        fnet_dll_implement(bool) isSysBlock(const error::Error& err) {
            if (err == error::block) {
                return true;
            }
            if (err.category() != error::Category::os) {
                return false;
            }
            if (err.type() != error::ErrorType::number) {
                return false;
            }
#ifdef FUTILS_PLATFORM_WINDOWS
            return err.code() == WSAEWOULDBLOCK;
#else
            return err.code() == EINPROGRESS || err.code() == EWOULDBLOCK;
#endif
        }

        constexpr auto errAddrNotSupport = error::Error("NetAddrPort type not supported by fnet", error::Category::lib, error::fnet_usage_error);

        expected<void> Socket::connect(const NetAddrPort& addr) {
            return get_raw().and_then([&](std::uintptr_t sock) -> expected<void> {
                sockaddr_storage st{};
                auto [a, len] = NetAddrPort_to_sockaddr(&st, addr);
                if (!a) {
                    return unexpect(errAddrNotSupport);
                }
                auto res = lazy::connect_(sock, a, len);
                if (res < 0) {
                    return unexpect(error::Errno());
                }
                return {};
            });
        }

        struct SelectError {
            error::Error err;

            void error(auto&& p) {
                strutil::append(p, "select error: hit exception fd: ");
                err.error(p);
            }
        };

        expected<void> sock_wait(std::uintptr_t sock, std::uint32_t sec, std::uint32_t usec, bool read) {
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
                return unexpect(error::Errno());
            }
#ifdef FUTILS_PLATFORM_WINDOWS
            if (lazy::__WSAFDIsSet_(sock, &excset)) {
                return unexpect(SelectError{error::Errno()});
            }
            if (lazy::__WSAFDIsSet_(sock, &xset)) {
                return {};
            }
#else
            if (FD_ISSET(sock, &excset)) {
                return unexpect(SelectError{error::Errno()});
            }
            if (FD_ISSET(sock, &xset)) {
                return {};
            }
#endif
            return unexpect(error::block);
        }

        expected<void> Socket::wait_readable(std::uint32_t sec, std::uint32_t usec) {
            return get_raw().and_then([&](std::uintptr_t sock) {
                return sock_wait(sock, sec, usec, true);
            });
        }

        expected<void> Socket::wait_writable(std::uint32_t sec, std::uint32_t usec) {
            return get_raw().and_then([&](std::uintptr_t sock) {
                return sock_wait(sock, sec, usec, false);
            });
        }

        expected<view::rvec> Socket::write(view::rvec data, int flag) {
            return get_raw().and_then([&](std::uintptr_t sock) -> expected<view::rvec> {
                auto sub = data.substr(0, (std::numeric_limits<socklen_t>::max)());
                auto res = lazy::send_(sock, sub.as_char(), sub.size(), flag);
                if (res < 0) {
                    return unexpect(error::Errno());
                }
                return data.substr(res);
            });
        }

        expected<view::wvec> Socket::read(view::wvec data, int flag, bool is_stream) {
            return get_raw().and_then([&](std::uintptr_t sock) -> expected<view::wvec> {
                auto sub = data.substr(0, (std::numeric_limits<socklen_t>::max)());
                auto res = lazy::recv_(sock, sub.as_char(), sub.size(), flag);
                if (res < 0) {
                    return unexpect(error::Errno());
                }
                if (res == 0 && is_stream) {
                    return unexpect(error::eof);
                }
                return data.substr(0, res);
            });
        }

        expected<view::rvec> Socket::writeto(const NetAddrPort& addr, view::rvec data, int flag) {
            return get_raw().and_then([&](std::uintptr_t sock) -> expected<view::rvec> {
                sockaddr_storage st;
                auto [addrptr, addrlen] = NetAddrPort_to_sockaddr(&st, addr);
                if (!addrptr) {
                    return unexpect(errAddrNotSupport);
                }
                auto sub = data.substr(0, (std::numeric_limits<socklen_t>::max)());
                auto res = lazy::sendto_(sock, sub.as_char(), sub.size(), flag, addrptr, addrlen);
                if (res < 0) {
                    return unexpect(error::Errno());
                }
                return data.substr(res);
            });
        }

        expected<view::rvec> Socket::writemsg(const NetAddrPort& addr, SockMsg<view::rvec> msg, int flag) {
            return get_raw().and_then([&](std::uintptr_t sock) -> expected<view::rvec> {
                sockaddr_storage st;
                auto [addrptr, addrlen] = NetAddrPort_to_sockaddr(&st, addr);
#ifdef FUTILS_PLATFORM_WINDOWS
                WSAMSG wmsg{};
                wmsg.name = (sockaddr*)addrptr;
                wmsg.namelen = addrlen;
                wmsg.Control.buf = (CHAR*)msg.control.as_char();
                wmsg.Control.len = msg.control.size();
                wmsg.dwBufferCount = 1;
                WSABUF buf{};
                buf.buf = (CHAR*)msg.data.as_char();
                buf.len = msg.data.size();
                wmsg.lpBuffers = &buf;
                wmsg.dwFlags = flag;
                DWORD dataTransferred = 0;
                auto res = lazy::WSASendMsg_(sock, &wmsg, flag, &dataTransferred, nullptr, nullptr);
                if (res != 0) {
                    return unexpect(error::Errno());
                }
                return msg.data.substr(0, dataTransferred);
#else
                struct msghdr hdr;
                hdr.msg_name = (void*)addrptr;
                hdr.msg_namelen = addrlen;
                hdr.msg_control = (void*)msg.control.as_char();
                hdr.msg_controllen = msg.control.size();
                struct iovec iov;
                iov.iov_base = (void*)msg.data.as_char();
                iov.iov_len = msg.data.size();
                hdr.msg_iov = &iov;
                hdr.msg_iovlen = 1;
                hdr.msg_flags = flag;
                auto res = lazy::sendmsg_(sock, &hdr, flag);
                if (res < 0) {
                    return unexpect(error::Errno());
                }
                return msg.data.substr(res);
#endif
            });
        }

        expected<std::pair<view::wvec, NetAddrPort>> Socket::readmsg(SockMsg<view::wvec> msg, int flag, bool is_stream) {
            return get_raw().and_then([&](std::uintptr_t sock) -> expected<std::pair<view::wvec, NetAddrPort>> {
                sockaddr_storage st{};
#ifdef FUTILS_PLATFORM_WINDOWS
                WSAMSG wmsg{};
                wmsg.name = (sockaddr*)&st;
                wmsg.namelen = sizeof(st);
                wmsg.Control.buf = (CHAR*)msg.control.as_char();
                wmsg.Control.len = msg.control.size();
                wmsg.dwBufferCount = 1;
                WSABUF buf{};
                buf.buf = (CHAR*)msg.data.as_char();
                buf.len = msg.data.size();
                wmsg.lpBuffers = &buf;
                wmsg.dwFlags = flag;
                DWORD dataTransferred = 0;
                auto res = lazy::WSARecvMsg_(sock, &wmsg, &dataTransferred, nullptr, nullptr);
                if (res != 0) {
                    return unexpect(error::Errno());
                }
                if (dataTransferred == 0 && is_stream) {
                    return unexpect(error::eof);
                }
                return std::make_pair(
                    msg.data.substr(0, dataTransferred),
                    wmsg.name ? sockaddr_to_NetAddrPort(wmsg.name, wmsg.namelen) : NetAddrPort{});
#else
                struct msghdr hdr;
                hdr.msg_name = (void*)&st;
                hdr.msg_namelen = sizeof(st);
                hdr.msg_control = (void*)msg.control.as_char();
                hdr.msg_controllen = msg.control.size();
                struct iovec iov;
                iov.iov_base = (void*)msg.data.as_char();
                iov.iov_len = msg.data.size();
                hdr.msg_iov = &iov;
                hdr.msg_iovlen = 1;
                hdr.msg_flags = flag;
                auto res = lazy::recvmsg_(sock, &hdr, flag);
                if (res < 0) {
                    return unexpect(error::Errno());
                }
                if (res == 0 && is_stream) {
                    return unexpect(error::eof);
                }
                return std::make_pair(
                    msg.data.substr(0, res),
                    hdr.msg_name ? sockaddr_to_NetAddrPort((sockaddr*)hdr.msg_name, hdr.msg_namelen) : NetAddrPort{});
#endif
            });
        }

        expected<std::pair<view::wvec, NetAddrPort>> Socket::readfrom(view::wvec data, int flag, bool is_stream) {
            return get_raw().and_then([&](std::uintptr_t sock) -> expected<std::pair<view::wvec, NetAddrPort>> {
                sockaddr_storage soct{};
                socklen_t fromlen = sizeof(soct);
                auto addr = reinterpret_cast<sockaddr*>(&soct);
                auto sub = data.substr(0, (std::numeric_limits<socklen_t>::max)());
                auto res = lazy::recvfrom_(sock, sub.as_char(), sub.size(), flag, addr, &fromlen);
                if (res < 0) {
                    return unexpect(error::Errno());
                }
                if (res == 0 && is_stream) {
                    return unexpect(error::eof);
                }
                return std::make_pair(
                    data.substr(0, res),
                    sockaddr_to_NetAddrPort(addr, fromlen));
            });
        }

#ifdef FUTILS_PLATFORM_WINDOWS
        constexpr auto shutdown_recv = SD_RECEIVE;
        constexpr auto shutdown_send = SD_SEND;
        constexpr auto shutdown_both = SD_BOTH;
#else
        constexpr auto shutdown_recv = SHUT_RD;
        constexpr auto shutdown_send = SHUT_WR;
        constexpr auto shutdown_both = SHUT_RDWR;
#endif

        expected<void> Socket::shutdown(ShutdownMode mode) {
            return get_raw().and_then([&](std::uintptr_t sock) -> expected<void> {
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
                    return unexpect(error::Errno());
                }
                return {};
            });
        }

        Socket make_socket(void* uptr) {
            return Socket(uptr);
        }

        expected<void> Socket::bind(const NetAddrPort& addr) {
            return get_raw().and_then([&](std::uintptr_t sock) -> expected<void> {
                ::sockaddr_storage storage;
                auto [addr_ptr, len] = NetAddrPort_to_sockaddr(&storage, addr);
                if (!addr_ptr) {
                    return unexpect(errAddrNotSupport);
                }
                auto res = lazy::bind_(sock, addr_ptr, len);
                if (res != 0) {
                    return unexpect(error::Errno());
                }
                return {};
            });
        }

        expected<void> Socket::listen(int back) {
            return get_raw().and_then([&](std::uintptr_t sock) -> expected<void> {
                auto res = lazy::listen_(sock, back);
                if (res != 0) {
                    return unexpect(error::Errno());
                }
                return {};
            });
        }

        expected<Socket> setup_socket(std::uintptr_t sock, event::IOEvent* event) {
            // prevent leak by exception
            auto d = helper::defer([&] { sockclose(sock); });
            set_nonblock(sock);
            auto tbl = make_sock_table(sock, event);
            d.cancel();  // now sock transferred to tbl
            auto registered = event->register_handle(sock, tbl);
            if (!registered) {
                decr_table(tbl);
                return registered.transform([] { return Socket(); });
            }
            return make_socket(tbl);
        }

        expected<std::pair<Socket, NetAddrPort>> Socket::accept() {
            return get_raw().and_then([&](std::uintptr_t sock) -> expected<std::pair<Socket, NetAddrPort>> {
                sockaddr_storage st{};
                socklen_t addrlen = sizeof(st);
                auto ptr = reinterpret_cast<sockaddr*>(&st);
                auto new_sock = lazy::accept_(sock, ptr, &addrlen);
                if (new_sock == -1) {
                    return unexpect(error::Errno());
                }
                // register IOCP
                return setup_socket(new_sock, static_cast<SockTable*>(ctx)->event)
                    .transform([&](Socket&& s) {
                        return std::make_pair(std::move(s), sockaddr_to_NetAddrPort(ptr, addrlen));
                    });
            });
        }

        Socket::~Socket() {
            decr_table(ctx);
        }

        expected<event::IOEvent>& init_event() {
            static expected<event::IOEvent> event = event::make_io_event(event::fnet_handle_completion, nullptr);
            return event;
        }

        fnet_dll_implement(expected<size_t>) wait_io_event(std::uint32_t time) {
            return init_event().and_then([&](event::IOEvent& event) {
                return event.wait(time);
            });
        }

        fnet_dll_implement(expected<Socket>) make_socket(SockAttr attr, event::IOEvent* event) {
            set_error(0);
            if (!lazy::load_socket()) {
                return unexpect(lazy::load_error("socket library not loaded"));
            }
            if (!event) {
                auto& events = init_event();
                if (!events) {
                    return events.transform([](event::IOEvent&) { return Socket(); });
                }
                event = events.value_ptr();
            }
#ifdef FUTILS_PLATFORM_WINDOWS
            auto sock = lazy::WSASocketW_(attr.address_family, attr.socket_type, attr.protocol, nullptr, 0, WSA_FLAG_OVERLAPPED);
#else
            auto sock = lazy::socket_(attr.address_family, attr.socket_type, attr.protocol);
#endif
            if (sock == -1) {
                return unexpect(error::Errno());
            }
            return setup_socket(sock, event);
        }

    }  // namespace fnet
}  // namespace futils
