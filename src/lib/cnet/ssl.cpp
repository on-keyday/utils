/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/cnet/ssl.h"
#include "../../include/wrap/light/string.h"
#include "../../include/number/array.h"
#include "../../include/helper/appender.h"
#ifndef USE_OPENSSL
#define USE_OPENSSL 1
#endif
#if USE_OPENSSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#endif

namespace utils {
    namespace cnet {
        namespace ssl {
#if !USE_OPENSSL
            CNet* STDCALL create_client() {
                return nullptr;
            }
#else
            template <class Char>
            using Host = number::Array<254, Char, true>;
            struct OpenSSLContext {
                ::SSL* ssl;
                ::SSL_CTX* sslctx;
                ::BIO* bio;
                wrap::string cert_file;
                number::Array<50, unsigned char, true> alpn{0};
                Host<char> host;
                bool setup = false;
                wrap::string buffer;
                size_t buf_index = 0;
                size_t write_index = 0;
                TLSStatus status;

                void append_to_buffer(const char* data, size_t size_) {
                    auto now_cap = buffer.size() - buf_index;
                    if (now_cap < size_) {
                        auto cur_size = buffer.size();
                        auto add_ = size_ - cur_size;
                        buffer.resize(cur_size + add_);
                    }
                    ::memmove_s(buffer.data() + buf_index, buffer.size(), data, size_);
                    buf_index += size_;
                }
            };

            int write_to_bio(CNet* ctx, OpenSSLContext* tls) {
                size_t w = 0;
                auto err = ::BIO_write_ex(tls->bio, tls->buffer.c_str(), tls->buf_index - tls->write_index, &w);
                if (!err) {
                    if (::BIO_should_retry(tls->bio)) {
                        return 0;
                    }
                    return -1;
                }
                tls->write_index += w;
                if (tls->write_index == tls->buf_index) {
                    tls->buf_index = 0;
                    tls->write_index = 0;
                    return 1;
                }
                return 0;
            }

            bool need_IO(OpenSSLContext* tls) {
                auto err = ::SSL_get_error(tls->ssl, -1);
                return err == SSL_ERROR_WANT_READ ||
                       err == SSL_ERROR_WANT_WRITE ||
                       err == SSL_ERROR_SYSCALL;
            }

            bool do_IO(CNet* ctx, OpenSSLContext* tls) {
                if (tls->buf_index) {
                    auto err = write_to_bio(ctx, tls);
                    if (err <= 0) {
                        return err == 0;
                    }
                }
                number::Array<1024, char> v;
                while (true) {
                    auto err = ::BIO_read_ex(tls->bio, v.buf, v.capacity(), &v.i);
                    tls->append_to_buffer(v.c_str(), v.size());
                    if (!err || v.i < v.capacity()) {
                        break;
                    }
                }
                auto lowconn = cnet::get_lowlevel_protocol(ctx);
                assert(lowconn);
                tls->status = TLSStatus::start_write_to_low;
                invoke_callback(ctx);
                size_t w = 0;
                while (true) {
                    if (!cnet::write(lowconn, tls->buffer.data(), tls->buf_index, &w)) {
                        return false;
                    }
                    if (w == tls->buffer.size()) {
                        break;
                    }
                    tls->status = TLSStatus::writing_to_low;
                    invoke_callback(ctx);
                }
                tls->status = TLSStatus::write_to_low_done;
                invoke_callback(ctx);
                tls->status = TLSStatus::start_read_from_low;
                invoke_callback(ctx);
                tls->buf_index = 0;
                if (tls->buffer.size() < 1024) {
                    tls->buffer.resize(1024);
                }
                while (true) {
                    if (!cnet::read(lowconn, tls->buffer.data(), tls->buffer.size(), &tls->buf_index)) {
                        return false;
                    }
                    if (tls->buf_index != 0) {
                        break;
                    }
                    tls->status = TLSStatus::reading_from_low;
                    invoke_callback(ctx);
                }
                tls->status = TLSStatus::read_from_low_done;
                invoke_callback(ctx);
                return write_to_bio(ctx, tls) >= 0;
            }

            bool open_tls(CNet* ctx, OpenSSLContext* tls) {
                auto lproto = cnet::get_lowlevel_protocol(ctx);
                if (!lproto) {
                    return false;
                }
                tls->sslctx = ::SSL_CTX_new(TLS_method());
                ::SSL_CTX_set_options(tls->sslctx, SSL_OP_NO_SSLv2);
                if (!tls->sslctx) {
                    return false;
                }
                if (!::SSL_CTX_load_verify_locations(tls->sslctx, tls->cert_file.c_str(), nullptr)) {
                    return false;
                }
                tls->ssl = ::SSL_new(tls->sslctx);
                if (!tls->ssl) {
                    return false;
                }
                ::BIO* ssl_side = nullptr;
                ::BIO_new_bio_pair(&ssl_side, 0, &tls->bio, 0);
                if (!ssl_side) {
                    return false;
                }
                ::SSL_set_bio(tls->ssl, ssl_side, ssl_side);
                if (!::SSL_set_alpn_protos(tls->ssl, tls->alpn.c_str(), tls->alpn.size())) {
                    return false;
                }
                ::SSL_set_tlsext_host_name(tls->ssl, tls->host.c_str());
                auto param = SSL_get0_param(tls->ssl);
                if (!X509_VERIFY_PARAM_add1_host(param, tls->host.c_str(), tls->host.size())) {
                    return false;
                }
                tls->status = TLSStatus::start_connect;
                invoke_callback(ctx);
                while (true) {
                    auto err = ::SSL_connect(tls->ssl);
                    if (err == 1) {
                        break;
                    }
                    else if (!err) {
                        return false;
                    }
                    else {
                        if (need_IO(tls)) {
                            if (!do_IO(ctx, tls)) {
                                return false;
                            }
                            continue;
                        }
                        return false;
                    }
                }
                tls->status = TLSStatus::connected;
                invoke_callback(ctx);
                tls->setup = true;
                return true;
            }

            void close_tls(CNet* ctx, OpenSSLContext* tls) {
                if (tls->setup) {
                    auto err = ::SSL_shutdown(tls->ssl);
                }
                ::SSL_free(tls->ssl);
                tls->ssl = nullptr;
                tls->bio = nullptr;
                ::SSL_CTX_free(tls->sslctx);
                tls->sslctx = nullptr;
                cnet::close(cnet::get_lowlevel_protocol(ctx));
            }

            template <class Callback>
            bool io_common_loop(CNet* ctx, OpenSSLContext* tls, Callback&& callback) {
                while (true) {
                    auto err = callback();
                    if (!err) {
                        if (need_IO(tls)) {
                            if (!do_IO(ctx, tls)) {
                                return false;
                            }
                            continue;
                        }
                        return false;
                    }
                    break;
                }
            }

            bool read_tls(CNet* ctx, OpenSSLContext* tls, Buffer<char>* buf) {
                tls->status = TLSStatus::start_read;
                invoke_callback(ctx);
                if (!io_common_loop(
                        ctx, tls,
                        [&]() {
                            return ::SSL_read_ex(tls->ssl, buf->ptr, buf->size, &buf->proced);
                        })) {
                    return false;
                }
                tls->status = TLSStatus::read_done;
                invoke_callback(ctx);
                return true;
            }

            bool write_tls(CNet* ctx, OpenSSLContext* tls, Buffer<const char>* buf) {
                tls->status = TLSStatus::start_write;
                invoke_callback(ctx);
                if (!io_common_loop(
                        ctx, tls,
                        [&]() {
                            return ::SSL_write_ex(tls->ssl, buf->ptr, buf->size, &buf->proced);
                        })) {
                    return false;
                }
                tls->status = TLSStatus::write_done;
                invoke_callback(ctx);
                return true;
            }

            bool tls_settings_ptr(CNet* ctx, OpenSSLContext* tls, std::int64_t key, void* value) {
                if (!value) {
                    return false;
                }
                if (key == host_verify) {
                    auto host = (const char*)value;
                    tls->host = {};
                    helper::append(tls->host, host);
                }
                else if (key == cert_file) {
                    auto file = (const char*)value;
                    tls->cert_file = file;
                }
                else if (key == alpn) {
                    auto alpn_str = (const char*)value;
                    tls->alpn = {};
                    helper::append(tls->alpn, alpn_str);
                }
                else {
                    return false;
                }
                return true;
            }

            std::int64_t tls_query_number(CNet* ctx, OpenSSLContext* tls, std::int64_t key) {
                if (key == protocol_type) {
                    return byte_stream;
                }
                else if (key == error_code) {
                    return ::SSL_get_error(tls->ssl, -1);
                }
                return 0;
            }

            void* tls_query_ptr(CNet* ctx, OpenSSLContext* tls, std::int64_t key) {
                if (key == protocol_name) {
                    return (void*)"tls";
                }
                else if (key == host_verify) {
                    return (void*)tls->host.c_str();
                }
                else if (key == cert_file) {
                    return (void*)tls->cert_file.c_str();
                }
                else if (key == raw_ssl) {
                    return tls->ssl;
                }
                return nullptr;
            }

            CNet* STDCALL create_client() {
                ProtocolSuite<OpenSSLContext> ssl_proto{
                    .initialize = open_tls,
                    .write = write_tls,
                    .read = read_tls,
                    .uninitialize = close_tls,
                    .settings_ptr = tls_settings_ptr,
                    .query_number = tls_query_number,
                    .query_ptr = tls_query_ptr,
                    .deleter = [](OpenSSLContext* v) { delete v; },
                };
                return create_cnet(CNetFlag::once_set_no_delete_link | CNetFlag::init_before_io, new OpenSSLContext{}, ssl_proto);
            }
#endif
        }  // namespace ssl
    }      // namespace cnet
}  // namespace utils
