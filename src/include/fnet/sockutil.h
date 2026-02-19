/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "socket.h"

namespace futils::fnet {

    void socket_write_common(Socket&& sock, view::rvec buf, auto&& call_async, auto&& then, auto&& error) {
        while (buf.size()) {
            auto d = sock.write(buf);
            if (!d) {
                if (isSysBlock(d.error())) {
                    call_async(std::move(sock), buf, std::forward<decltype(then)>(then), std::forward<decltype(error)>(error));
                    return;
                }
                error(d.error());
                return;
            }
            buf = *d;
        }
        then(std::move(sock));
    }

    void default_write_async(Socket&& sock, view::rvec buf, auto&& then, auto&& error) {
        auto r = sock.write_async(async_then(
            BufferManager<view::rvec>{buf},
            [=](Socket&& sock, BufferManager<view::rvec> buf, NotifyResult&& r) {
                auto res = r.write_unwrap(buf.get_buffer(), [&](view::rvec w) {
                    return sock.write(w);
                });
                if (!res) {
                    error(res.error());
                    return;
                }
                socket_write(std::move(sock), *res,
                             std::forward<decltype(then)>(then),
                             std::forward<decltype(error)>(error));
            }));
        if (!r) {
            error(r.error());
            return;
        }
        // at here, we need to only wait and nothing to do, so we just return
    }

    void socket_write(Socket&& sock, view::rvec buf, auto&& then, auto&& error) {
        return socket_write_common(
            std::move(sock), buf,
            [](Socket&& sock, view::rvec buf, auto&& then, auto&& error) {
                default_write_async(std::move(sock), buf, then, error);
            },
            then, error);
    }

    void default_write_async_deferred(Socket&& sock, view::rvec buf, auto&& notify, auto&& then, auto&& error) {
        auto r = sock.write_async_deferred(async_notify_then(
            BufferManager<view::rvec>(buf),
            std::forward<decltype(notify)>(notify),
            [notify, then, error](Socket&& sock, BufferManager<view::rvec> buf, NotifyResult&& r) {
                auto res = r.write_unwrap(buf.get_buffer(), [&](view::rvec w) {
                    return sock.write(w);
                });
                if (!res) {
                    error(res.error());
                    return;
                }
                socket_write_deferred(std::move(sock), *res,
                                      notify, then, error);
            }));
        if (!r) {
            error(r.error());
            return;
        }
        // at here, we need to only wait and nothing to do, so we just return
    }

    void socket_write_deferred(Socket&& sock, view::rvec buf, auto&& notify, auto&& then, auto&& error) {
        return socket_write_common(
            std::move(sock), buf,
            [&](Socket&& sock, view::rvec buf, auto&& then, auto&& error) {
                default_write_async_deferred(std::move(sock), buf, std::forward<decltype(notify)>(notify), std::forward<decltype(then)>(then), std::forward<decltype(error)>(error));
            },
            then, error);
    }

    void socket_read_common(Socket&& sock, auto&& add_data, auto&& call_async, auto&& then, auto&& error, view::wvec recv_buf = {}, bool anyway_then = false) {
        auto r = sock.read_until_block(
            add_data,
            recv_buf);
        if (!r) {
            error(r.error());
            return;
        }
        if (*r == 0 && !anyway_then) {
            call_async(std::move(sock), std::forward<decltype(add_data)>(add_data), std::forward<decltype(then)>(then), std::forward<decltype(error)>(error));
            return;
        }
        then(std::move(sock));
    }

    auto default_read_async(Socket&& sock, auto&& buf_manager, auto&& add_data, auto&& then, auto&& error) {
        auto r = sock.read_async(async_then(
            std::forward<decltype(buf_manager)>(buf_manager),
            [=](Socket&& sock, auto&& buf, NotifyResult&& r) {
                auto res = r.read_unwrap(buf.get_buffer(), [&](view::wvec w) {
                    return sock.read(w);
                });
                if (!res) {
                    error(res.error());
                    return;
                }
                add_data(*res);
                socket_read(std::move(sock), std::forward<decltype(add_data)>(add_data),
                            std::forward<decltype(then)>(then),
                            std::forward<decltype(error)>(error),
                            buf.get_buffer(), true);
            }));
        if (!r) {
            error(r.error());
            return;
        }
        // at here, we need to only wait and nothing to do, so we just return
    }

    void socket_read(Socket&& sock, auto&& add_data, auto&& then, auto&& error, view::wvec recv_buffer = {}, bool anyway_then = false) {
        return socket_read_common(
            std::move(sock), std::forward<decltype(add_data)>(add_data),
            [&](Socket&& sock, auto&& add_data, auto&& then, auto&& error) {
                if (recv_buffer.size()) {
                    default_read_async(std::move(sock), BufferManager<view::wvec>{recv_buffer}, std::forward<decltype(add_data)>(add_data),
                                       std::forward<decltype(then)>(then),
                                       std::forward<decltype(error)>(error));
                }
                else {
                    default_read_async(std::move(sock), BufferManager<byte[1024]>{},
                                       std::forward<decltype(add_data)>(add_data),
                                       std::forward<decltype(then)>(then),
                                       std::forward<decltype(error)>(error));
                }
            },
            then, error, recv_buffer, anyway_then);
    }

    void default_read_async_deferred(Socket&& sock, auto&& buf_manager, auto&& add_data, auto&& notify, auto&& then, auto&& error) {
        auto r = sock.read_async_deferred(async_notify_then(
            std::forward<decltype(buf_manager)>(buf_manager),
            std::forward<decltype(notify)>(notify),
            [=](Socket&& sock, auto&& buf, NotifyResult&& r) {
                auto res = r.read_unwrap(buf.get_buffer(), [&](view::wvec w) {
                    return sock.read(w);
                });
                if (!res) {
                    error(res.error());
                    return;
                }
                add_data(*res);
                socket_read_deferred(std::move(sock), add_data,
                                     notify,
                                     then,
                                     error,
                                     buf.get_buffer(), true);
            }));
        if (!r) {
            error(r.error());
            return;
        }
        // at here, we need to only wait and nothing to do, so we just return
    }

    void socket_read_deferred(Socket&& sock, auto&& add_data, auto&& notify, auto&& then, auto&& error, view::wvec recv_buffer = {}, bool anyway_then = false) {
        return socket_read_common(
            std::move(sock), std::forward<decltype(add_data)>(add_data),
            [&](Socket&& sock, auto&& add_data, auto&& then, auto&& error) {
                if (recv_buffer.size()) {
                    default_read_async_deferred(std::move(sock), BufferManager<view::wvec>{recv_buffer}, std::forward<decltype(add_data)>(add_data),
                                                std::forward<decltype(notify)>(notify),
                                                std::forward<decltype(then)>(then),
                                                std::forward<decltype(error)>(error));
                }
                else {
                    default_read_async_deferred(std::move(sock), BufferManager<byte[1024]>{}, std::forward<decltype(add_data)>(add_data),
                                                std::forward<decltype(notify)>(notify),
                                                std::forward<decltype(then)>(then),
                                                std::forward<decltype(error)>(error));
                }
            },
            then, error, recv_buffer);
    }

}  // namespace futils::fnet
