/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// ssl_async - async ssl operation
#include "ssl_internal.h"
#include "../../../include/net/async/pool.h"

namespace utils {
    namespace net {
        namespace internal {
            bool SSLAsyncImpl::do_IO(async::Context& ctx) {
                if (buffer.size()) {
                    auto f = write_to_ssl(buffer);
                    if (f == szfailed) {
                        return false;
                    }
                    if (buffer.size()) {
                        return true;
                    }
                }
                read_from_ssl(buffer);
                auto w = io.write(buffer.c_str(), buffer.size());
                w.wait_or_suspend(ctx);
                buffer.resize(1024);
                auto recv = io.read(buffer.data(), 1024);
                recv.wait_or_suspend(ctx);
                auto data = recv.get();
                buffer.resize(data.read);
                auto f = write_to_ssl(buffer);
                if (f == szfailed) {
                    return false;
                }
                return true;
            }

            struct SSLSet {
                static void set(SSLAsyncConn& conn, SSLAsyncImpl* ptr) {
                    conn.impl = ptr;
                }
            };
        }  // namespace internal

        async::Future<ReadInfo> SSLAsyncConn::read(char* ptr, size_t size) {
            if (!ptr || !size) {
                return nullptr;
            }
            return get_pool().start<ReadInfo>(
                [=, this, lock = impl->conn.lock()](async::Context& ctx) {
                    size_t red = 0;
                    while (!::SSL_read_ex(impl->ssl, ptr, size, &red)) {
                        if (need_io(impl->ssl)) {
                            if (!impl->do_IO(ctx)) {
                                ctx.set_value(ReadInfo{
                                    .byte = ptr,
                                    .size = size,
                                    .err = ::SSL_get_error(impl->ssl, -1),
                                });
                            }
                            continue;
                        }
                        ctx.set_value(ReadInfo{
                            .byte = ptr,
                            .size = size,
                            .err = ::SSL_get_error(impl->ssl, -1),
                        });
                        return;
                    }
                    ctx.set_value(ReadInfo{.byte = ptr, .size = size, .read = red});
                });
        }

        async::Future<WriteInfo> SSLAsyncConn::write(const char* ptr, size_t size) {
            if (!ptr || !size) {
                return nullptr;
            }
            return get_pool().start<WriteInfo>(
                [=, this, lock = impl->conn.lock()](async::Context& ctx) {
                    size_t w = 0;
                    while (!::SSL_write_ex(impl->ssl, ptr, size, &w)) {
                        if (need_io(impl->ssl)) {
                            if (!impl->do_IO(ctx)) {
                                ctx.set_value(WriteInfo{
                                    .byte = ptr,
                                    .size = size,
                                    .err = ::SSL_get_error(impl->ssl, -1),
                                });
                                return;
                            }
                            continue;
                        }
                        ctx.set_value(WriteInfo{
                            .byte = ptr,
                            .size = size,
                            .err = ::SSL_get_error(impl->ssl, -1),
                        });
                        return;
                    }
                    ctx.set_value(WriteInfo{.byte = ptr, .size = size, .written = w});
                });
        }

        bool common_setup_async(internal::SSLAsyncImpl* impl, AsyncIOClose&& io, const char* cert, const char* alpn, const char* host,
                                const char* selfcert, const char* selfprivate) {
            if (!common_setup_sslctx(impl, cert, selfcert, selfprivate)) {
                return false;
            }
            if (io) {
                if (!setup_ssl(impl)) {
                    return false;
                }
                impl->io = std::move(io);
            }
            if (!common_setup_ssl(impl, alpn, host)) {
                return false;
            }
            return true;
        }

        State SSLAsyncConn::close(bool force) {
            get_pool().post([this, lock = impl->conn.lock()](async::Context& ctx) {
                if (impl->connected) {
                    size_t times = 0;
                    while (times < 10) {
                        auto done = ::SSL_shutdown(impl->ssl);
                        if (done < 0) {
                            if (need_io(impl->ssl)) {
                                if (!impl->do_IO(ctx)) {
                                    break;
                                }
                            }
                        }
                        else if (done) {
                            break;
                        }
                    }
                }
                impl->clear();
                impl->io.close(true);
                impl->io = nullptr;
            });
            return State::complete;
        }

        async::Future<wrap::shared_ptr<SSLAsyncConn>> STDCALL open_async(
            AsyncIOClose&& io,
            const char* cert, const char* alpn, const char* host,
            const char* selfcert, const char* selfprivate) {
            if (!io || !cert) {
                return nullptr;
            }
            auto impl = new internal::SSLAsyncImpl{};
            if (!common_setup_async(impl, std::move(io), cert, alpn, host, selfcert, selfprivate)) {
                return nullptr;
            }
            return get_pool().start<wrap::shared_ptr<SSLAsyncConn>>([impl](async::Context& ctx) {
                while (!::SSL_connect(impl->ssl)) {
                    if (need_io(impl->ssl)) {
                        if (!impl->do_IO(ctx)) {
                            return;
                        }
                        continue;
                    }
                    return;
                }
                auto as = wrap::make_shared<SSLAsyncConn>();
                internal::SSLSet::set(*as, impl);
                ctx.set_value(std::move(as));
            });
        }
    }  // namespace net
}  // namespace utils