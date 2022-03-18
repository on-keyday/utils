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
            int SSLAsyncImpl::do_IO(async::Context& ctx) {
                if (buffer.size()) {
                    auto f = write_to_ssl(buffer);
                    if (f == szfailed) {
                        return -1;
                    }
                    if (buffer.size()) {
                        return 0;
                    }
                }
                read_from_ssl(buffer);
                auto w = io.write(buffer.c_str(), buffer.size());
                w.wait_until(ctx);
                if (auto got = w.get(); got.err) {
                    return got.err;
                }
                buffer.resize(1024);
                auto recv = io.read(buffer.data(), 1024);
                recv.wait_until(ctx);
                auto data = recv.get();
                if (data.err) {
                    return data.err;
                }
                buffer.resize(data.read);
                auto f = write_to_ssl(buffer);
                if (f == szfailed) {
                    return -1;
                }
                return 0;
            }

            struct SSLSet {
                static void set(SSLAsyncConn& conn, SSLAsyncImpl* ptr) {
                    conn.impl = ptr;
                }
                static SSLAsyncImpl* get(SSLAsyncConn& conn) {
                    return conn.impl;
                }
            };
        }  // namespace internal

        SSLAsyncConn::~SSLAsyncConn() {
            close(true);
        }

        async::Future<ReadInfo> SSLAsyncConn::read(char* ptr, size_t size) {
            if (!ptr || !size) {
                return nullptr;
            }
            return get_pool().start<ReadInfo>(
                [=, this, lock = impl->conn.lock()](async::Context& ctx) {
                    size_t red = 0;
                    while (!::SSL_read_ex(impl->ssl, ptr, size, &red)) {
                        if (need_io(impl->ssl)) {
                            if (auto err = impl->do_IO(ctx); err != 0) {
                                ctx.set_value(ReadInfo{
                                    .byte = ptr,
                                    .size = size,
                                    .err = err,
                                });
                                return;
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

        void* SSLAsyncConn::get_raw_ssl() {
            if (!impl) {
                return nullptr;
            }
            return impl->ssl;
        }

        void* SSLAsyncConn::get_raw_sslctx() {
            if (!impl) {
                return nullptr;
            }
            return impl->ctx;
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
                            if (auto err = impl->do_IO(ctx); err != 0) {
                                ctx.set_value(WriteInfo{
                                    .byte = ptr,
                                    .size = size,
                                    .err = err,
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
                        SSL_ERROR_SSL;
                        return;
                    }
                    ctx.set_value(WriteInfo{.byte = ptr, .size = size, .written = w});
                });
        }

        SSLAsyncError common_setup_async(internal::SSLAsyncImpl* impl, AsyncIOClose&& io, const char* cert, const char* alpn, const char* host,
                                         const char* selfcert, const char* selfprivate) {
            if (auto err = common_setup_sslctx(impl, cert, selfcert, selfprivate, alpn);
                err != SSLAsyncError::none) {
                return err;
            }
            if (io) {
                if (!setup_ssl(impl)) {
                    return SSLAsyncError::set_up_error;
                }
                impl->io = std::move(io);
            }
            if (!common_setup_ssl(impl, host)) {
                return SSLAsyncError::host_register_error;
            }
            return SSLAsyncError::none;
        }

        State SSLAsyncConn::close(bool force) {
            get_pool().post([this, lock = impl->conn.lock()](async::Context& ctx) mutable {
                if (impl->connected) {
                    size_t times = 0;
                    while (times < 10) {
                        auto done = ::SSL_shutdown(impl->ssl);
                        if (done < 0) {
                            if (need_io(impl->ssl)) {
                                if (auto err = impl->do_IO(ctx); err != 0) {
                                    break;
                                }
                            }
                        }
                        else if (done) {
                            break;
                        }
                    }
                    impl->connected = false;
                }
                impl->clear();
                impl->io.close(true);
                impl->io = nullptr;
            });
            return State::complete;
        }

        const char* SSLAsyncConn::alpn_selected(int* len) {
            if (!impl || !impl->ssl) {
                return nullptr;
            }
            const unsigned char* selected = nullptr;
            unsigned int lens = 0;
            ::SSL_get0_alpn_selected(impl->ssl, &selected, &lens);
            if (len) {
                *len = (int)lens;
            }
            return (const char*)selected;
        }

        bool SSLAsyncConn::verify() {
            auto verify = ::SSL_get_peer_certificate(impl->ssl);
            if (!verify) {
                return false;
            }
            if (::SSL_get_verify_result(impl->ssl) != X509_V_OK) {
                return false;
            }
            ::X509_free(verify);
            return true;
        }

        async::Future<SSLAsyncResult> STDCALL open_async(
            AsyncIOClose&& io,
            const char* cert, const char* alpn, const char* host,
            const char* selfcert, const char* selfprivate) {
            if (!io || !cert) {
                return nullptr;
            }
            auto impl = new internal::SSLAsyncImpl{};
            if (auto err = common_setup_async(impl, std::move(io), cert, alpn, host, selfcert, selfprivate);
                err != SSLAsyncError::none) {
                return SSLAsyncResult{.err = err};
            }
            return start(
                [](async::Context& ctx, internal::SSLAsyncImpl* impl) -> SSLAsyncResult {
                    while (::SSL_connect(impl->ssl) < 0) {
                        if (need_io(impl->ssl)) {
                            if (auto code = impl->do_IO(ctx); code != 0) {
                                return SSLAsyncResult{
                                    .err = SSLAsyncError::connect_error,
                                    .errcode = ::SSL_get_error(impl->ssl, -1),
                                    .transporterr = code,
                                };
                            }
                            continue;
                        }
                        return SSLAsyncResult{
                            .err = SSLAsyncError::connect_error,
                            .errcode = ::SSL_get_error(impl->ssl, -1),
                        };
                    }
                    auto as = wrap::make_shared<SSLAsyncConn>();
                    internal::SSLSet::set(*as, impl);
                    impl->conn = as;
                    auto sel = as->alpn_selected(nullptr);
                    return SSLAsyncResult{.conn = as};
                },
                impl);
        }
    }  // namespace net
}  // namespace utils
