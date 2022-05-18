/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/platform/windows/dllexport_source.h"
#include "../../include/cnet/ssl.h"
#include "../../include/wrap/light/string.h"
#include "../../include/number/array.h"
#include "../../include/helper/appender.h"
#include "../../include/testutil/timer.h"
#ifndef USE_OPENSSL
#define USE_OPENSSL 1
#endif
#if USE_OPENSSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#define OPENSSL_THREAD_DEFINES
#include <openssl/opensslconf.h>
#include <openssl/crypto.h>
#endif

namespace utils {
    namespace cnet {
        namespace ssl {
#if !USE_OPENSSL
            CNet* STDCALL create_client() {
                return nullptr;
            }
#else

#define TLSLOG(ctx, record, msg, code) record.log(logobj{__FILE__, __LINE__, "tls", msg, ctx, (std::int64_t)code})

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
                time_t read_timeout = 0;
                bool loose_check = false;

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

            struct sslconsterror : consterror {
                void error(pushbacker pb) {
                    helper::appends(pb, "ssl/tls: ", this->msg);
                }
            };

            struct sslwraperror : consterror {
                Error w;
                void error(pushbacker pb) {
                    consterror::error(pb);
                    helper::append(pb, ": ");
                    w.error(pb);
                }

                Error unwrap() {
                    return std::move(w);
                }
            };

            struct ssloperror : sslconsterror {
                const char* op;
                void error(pushbacker pb) {
                    sslconsterror::error(pb);
                    helper::appends(pb, ": ", op);
                }
            };

            struct sslcodeerror : sslconsterror {
                int code;
            };

            int write_to_bio(CNet* ctx, OpenSSLContext* tls) {
                if (tls->buf_index == 0) {
                    return 1;
                }
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
                       err == SSL_ERROR_WANT_WRITE;
            }

            Error do_IO(Stopper stop, CNet* ctx, OpenSSLContext* tls, test::Timer* timer, bool shutdown_mode = false) {
                if (tls->buf_index) {
                    auto err = write_to_bio(ctx, tls);
                    if (err <= 0) {
                        if (err == 0) {
                            return nil();
                        }
                        return sslcodeerror{"failed to write", ::SSL_get_error(tls->ssl, -1)};
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
                TLSLOG(ctx, stop, "start writing low level connection", TLSStatus::start_write_to_low);
                size_t w = 0;
                while (true) {
                    if (auto err = cnet::write(stop, lowconn, tls->buffer.data(), tls->buf_index, &w);
                        err != nullptr) {
                        return err;
                    }
                    if (w == tls->buf_index) {
                        break;
                    }
                    tls->status = TLSStatus::writing_to_low;
                    TLSLOG(ctx, stop, "writing to low level connection", TLSStatus::writing_to_low);
                }
                tls->status = TLSStatus::write_to_low_done;
                TLSLOG(ctx, stop, "writing low level connection done", TLSStatus::write_to_low_done);
                tls->status = TLSStatus::start_read_from_low;
                TLSLOG(ctx, stop, "start reading low level connection", TLSStatus::start_read_from_low);
                tls->buf_index = 0;
                if (tls->buffer.size() < 1024) {
                    tls->buffer.resize(1024);
                }
                size_t count = 0;
                while (true) {
                    if (auto err = cnet::read(stop, lowconn, tls->buffer.data(), tls->buffer.size(), &tls->buf_index);
                        err != nullptr) {
                        return err;
                    }
                    if (tls->buf_index != 0) {
                        break;
                    }
                    if (shutdown_mode) {
                        count++;
                        if (count > 200) {
                            break;
                        }
                    }
                    tls->status = TLSStatus::reading_from_low;
                    TLSLOG(ctx, stop, "reading from low level connection", TLSStatus::reading_from_low);
                    if (tls->read_timeout && timer) {
                        if (timer->delta().count() >= tls->read_timeout) {
                            break;
                        }
                    }
                }
                tls->status = TLSStatus::read_from_low_done;
                TLSLOG(ctx, stop, "reading from low level connection done", TLSStatus::read_from_low_done);
                if (write_to_bio(ctx, tls) < 0) {
                    return sslcodeerror{"failed to write to ssl conn", ::SSL_get_error(tls->ssl, -1)};
                }
                return nil();
            }

            Error open_tls(Stopper stop, CNet* ctx, OpenSSLContext* tls) {
                auto lproto = cnet::get_lowlevel_protocol(ctx);
                if (!lproto) {
                    return sslconsterror{"low level protocol not set"};
                }
                tls->sslctx = ::SSL_CTX_new(TLS_method());
                if (!tls->sslctx) {
                    return sslconsterror{"SSL_CTX_new returns nullptr"};
                }
                ::SSL_CTX_set_options(tls->sslctx, SSL_OP_NO_SSLv2);
                if (!::SSL_CTX_load_verify_locations(tls->sslctx, tls->cert_file.c_str(), nullptr)) {
                    return ssloperror{"failed to load cert file", tls->cert_file.c_str()};
                }
                tls->ssl = ::SSL_new(tls->sslctx);
                if (!tls->ssl) {
                    return sslconsterror{"SSL_new returns nullptr"};
                }
                ::BIO* ssl_side = nullptr;
                ::BIO_new_bio_pair(&ssl_side, 0, &tls->bio, 0);
                if (!ssl_side) {
                    return sslconsterror{"BIO_new_bio_pair set nullptr"};
                }
                ::SSL_set_bio(tls->ssl, ssl_side, ssl_side);
                if (tls->alpn.size()) {
                    if (::SSL_set_alpn_protos(tls->ssl, tls->alpn.c_str(), tls->alpn.size()) != 0) {
                        return ssloperror{"failed to register alpn protocols", (const char*)tls->alpn.c_str()};
                    }
                }
                if (tls->host.size()) {
                    ::SSL_set_tlsext_host_name(tls->ssl, tls->host.c_str());
                    auto param = SSL_get0_param(tls->ssl);
                    if (!X509_VERIFY_PARAM_add1_host(param, tls->host.c_str(), tls->host.size())) {
                        return ssloperror{"failed to register host name", tls->host.c_str()};
                    }
                }
                tls->status = TLSStatus::start_connect;
                TLSLOG(ctx, stop, "start ssl connect", TLSStatus::start_connect);
                while (true) {
                    auto err = ::SSL_connect(tls->ssl);
                    if (err == 1) {
                        break;
                    }
                    else if (!err) {
                        return sslcodeerror{"SSL_connect failed", ::SSL_get_error(tls->ssl, 0)};
                    }
                    else {
                        if (need_IO(tls)) {
                            if (!do_IO(stop, ctx, tls, nullptr)) {
                                return sslconsterror{"do_IO failed"};
                            }
                            continue;
                        }
                        return sslcodeerror{"SSL_connect failed", ::SSL_get_error(tls->ssl, -1)};
                    }
                }
                if (!tls->loose_check) {
                    auto result = ::SSL_get_verify_result(tls->ssl);
                    auto cert = ::SSL_get_peer_certificate(tls->ssl);
                    if (result != X509_V_OK || !cert) {
                        return sslcodeerror{"verfication of certificate failed", result};
                    }
                    ::X509_free(cert);
                }
                tls->status = TLSStatus::connected;
                TLSLOG(ctx, stop, "ssl connect done", TLSStatus::connected);
                tls->setup = true;
                return nil();
            }

            void close_tls(Stopper stop, CNet* ctx, OpenSSLContext* tls) {
                size_t count = 0;
                while (tls->setup && count < 200) {
                    count++;
                    auto err = ::SSL_shutdown(tls->ssl);
                    if (err == -1 && need_IO(tls)) {
                        if (!do_IO(stop, ctx, tls, nullptr, true)) {
                            break;
                        }
                        continue;
                    }
                    else if (err == 0) {
                        number::Array<1024, char> tmpbuf;
                        err = SSL_read_ex(tls->ssl, tmpbuf.buf, tmpbuf.capacity(), &tmpbuf.i);
                        if (!err) {
                            if (need_IO(tls)) {
                                if (!do_IO(stop, ctx, tls, nullptr, true)) {
                                    break;
                                }
                                continue;
                            }
                            break;
                        }
                        continue;
                    }
                    break;
                }
                ::SSL_free(tls->ssl);
                tls->ssl = nullptr;
                tls->bio = nullptr;
                ::SSL_CTX_free(tls->sslctx);
                tls->sslctx = nullptr;
                cnet::close(stop, cnet::get_lowlevel_protocol(ctx));
            }

            template <class Callback>
            Error io_common_loop(Stopper stop, CNet* ctx, OpenSSLContext* tls, test::Timer* timer, Callback&& callback) {
                while (true) {
                    auto err = callback();
                    if (!err) {
                        if (need_IO(tls)) {
                            if (auto err = do_IO(stop, ctx, tls, timer); err != nullptr) {
                                return err;
                            }
                            if (timer && timer->delta().count() >= tls->read_timeout) {
                                break;
                            }
                            continue;
                        }
                        tls->setup = false;
                        return sslcodeerror{"operation failed at code", ::SSL_get_error(tls->ssl, 0)};
                    }
                    break;
                }
                return nil();
            }

            Error read_tls(Stopper stop, CNet* ctx, OpenSSLContext* tls, Buffer<char>* buf) {
                tls->status = TLSStatus::start_read;
                TLSLOG(ctx, stop, "start reading ssl", TLSStatus::start_read);
                test::Timer timer;
                if (auto err = io_common_loop(
                        stop, ctx, tls, &timer,
                        [&]() {
                            return ::SSL_read_ex(tls->ssl, buf->ptr, buf->size, &buf->proced);
                        });
                    err != nullptr) {
                    return sslwraperror{"SSL_read_ex failed", std::move(err)};
                }
                tls->status = TLSStatus::read_done;
                TLSLOG(ctx, stop, "reading ssl done", TLSStatus::read_done);
                return nil();
            }

            Error write_tls(Stopper stop, CNet* ctx, OpenSSLContext* tls, Buffer<const char>* buf) {
                tls->status = TLSStatus::start_write;
                TLSLOG(ctx, stop, "start writing ssl", TLSStatus::start_write);
                if (auto err = io_common_loop(
                        stop, ctx, tls, nullptr,
                        [&]() {
                            return ::SSL_write_ex(tls->ssl, buf->ptr, buf->size, &buf->proced);
                        });
                    err != nullptr) {
                    return sslwraperror{"SSL_write_ex failed", std::move(err)};
                }
                tls->status = TLSStatus::write_done;
                TLSLOG(ctx, stop, "writing ssl done", TLSStatus::write_done);
                return nil();
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
                else if (key == invoke_ssl_error_callback) {
                    auto cb = (ErrorCallback*)(value);
                    ::ERR_print_errors_cb(cb->cb, cb->user);
                    return true;
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
                else if (key == current_state) {
                    return (std::int64_t)tls->status;
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
                else if (key == alpn_selected) {
                    const unsigned char* ptr = nullptr;
                    unsigned int len = 0;
                    ::SSL_get0_alpn_selected(tls->ssl, &ptr, &len);
                    return (void*)ptr;
                }
                return nullptr;
            }

            bool tls_settings_number(CNet* ctx, OpenSSLContext* tls, std::int64_t key, std::int64_t value) {
                if (key == reading_timeout) {
                    tls->read_timeout = value;
                }
                else if (key == multi_thread) {
#ifdef OPENSSL_THREADS
                    return true;
#else
                    return false;
#endif
                }
                else if (key == looser_check) {
                    tls->loose_check = bool(value);
                }
                else {
                    return false;
                }
                return true;
            }

            CNet* STDCALL create_client() {
                ProtocolSuite<OpenSSLContext> ssl_proto{
                    .initialize = open_tls,
                    .write = write_tls,
                    .read = read_tls,
                    .uninitialize = close_tls,
                    .settings_number = tls_settings_number,
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
