/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "ssl_internal.h"

namespace utils {
    namespace net {

        namespace internal {
#ifndef USE_OPENSSL
            State SSLImpl::do_IO() {
                return State::undefined;
            }
            void SSLImpl::clear() {}
            SSLImpl::~SSLImpl() {}
#else

            size_t SSLImpl::read_from_ssl(wrap::string& buffer) {
                while (true) {
                    char tmpbuf[1024];
                    size_t red;
                    auto result = ::BIO_read_ex(bio, tmpbuf, 1024, &red);
                    buffer.append(tmpbuf, red);
                    if (!result || red < 1024) {
                        break;
                    }
                }
                return buffer.size();
            }

            size_t SSLImpl::write_to_ssl(wrap::string& buffer) {
                if (buffer.size()) {
                    size_t w = 0;
                    auto err = ::BIO_write_ex(bio, buffer.c_str(), buffer.size(), &w);
                    if (!err) {
                        if (!::BIO_should_retry(bio)) {
                            return szfailed;
                        }
                    }
                    else {
                        if (w < buffer.size()) {
                            buffer.erase(0, w);
                            return w;
                        }
                        buffer.clear();
                    }
                }
                return 0;
            }

            State SSLSyncImpl::do_IO() {
                bool has_wbuf = buffer.size() != 0;
                if (has_wbuf) {
                    iophase = SSLIOPhase::write_to_ssl;
                }
            BEGIN:
                if (iophase == SSLIOPhase::read_from_ssl) {
                    read_from_ssl(buffer);
                    iophase = SSLIOPhase::write_to_conn;
                }
                if (iophase == SSLIOPhase::write_to_conn) {
                    if (buffer.size()) {
                        auto e = io.write(buffer.c_str(), buffer.size(), nullptr);
                        if (e != State::complete) {
                            return e;
                        }
                        buffer.clear();
                    }
                    iophase = SSLIOPhase::read_from_conn;
                }
                if (iophase == SSLIOPhase::read_from_conn) {
                    auto err = read(buffer, io);
                    if (err != State::complete && err != State::running) {
                        return err;
                    }
                    if (buffer.size()) {
                        iophase = SSLIOPhase::write_to_ssl;
                    }
                    else {
                        iophase = SSLIOPhase::read_from_ssl;
                    }
                }
                if (iophase == SSLIOPhase::write_to_ssl) {
                    auto res = write_to_ssl(buffer);
                    if (res == szfailed) {
                        return State::failed;
                    }
                    iophase = SSLIOPhase::read_from_ssl;
                    if (has_wbuf) {
                        has_wbuf = false;
                        goto BEGIN;
                    }
                }
                return State::complete;
            }

            void SSLImpl::clear() {
                connected = false;
                ::SSL_free(ssl);
                ssl = nullptr;
                ::BIO_free(bio);
                bio = nullptr;
                // io = nullptr;
                buffer.clear();
            }

            SSLImpl::~SSLImpl() {
                clear();
                ::SSL_CTX_free(ctx);
            }
#endif

        }  // namespace internal

#ifdef USE_OPENSSL

        State close_impl(internal::SSLSyncImpl* impl) {
            if (!impl) {
                return State::failed;
            }
            impl->io_mode = internal::IOMode::close;
            if (impl->connected) {
                size_t times = 0;
            BEGIN:
                times++;
                auto res = ::SSL_shutdown(impl->ssl);
                if (res < 0) {
                    if (need_io(impl->ssl)) {
                        auto err = impl->do_IO();
                        if (err == State::running) {
                            return State::running;
                        }
                        else if (err == State::complete) {
                            if (times <= 10) {
                                goto BEGIN;
                            }
                        }
                    }
                }
                else if (res == 0) {
                    if (times <= 10) {
                        goto BEGIN;
                    }
                }
            }
            impl->clear();
            return State::complete;
        }

        bool need_io(::SSL* ssl) {
            auto errcode = ::SSL_get_error(ssl, -1);
            return errcode == SSL_ERROR_WANT_READ || errcode == SSL_ERROR_WANT_WRITE || errcode == SSL_ERROR_SYSCALL;
        }

        bool setup_ssl(internal::SSLImpl* impl) {
            ::BIO* tmpbio = nullptr;
            if (!::BIO_new_bio_pair(&impl->bio, 0, &tmpbio, 0)) {
                return false;
            }
            impl->ssl = ::SSL_new(impl->ctx);
            if (!impl->ssl) {
                ::BIO_free_all(impl->bio);
                impl->bio = nullptr;
                // impl->tmpbio = nullptr;
                return false;
            }
            ::SSL_set_bio(impl->ssl, tmpbio, tmpbio);
            return true;
        }

        SSLAsyncError common_setup_sslctx(internal::SSLImpl* impl, const char* cert, const char* selfcert, const char* selfprivate) {
            if (!impl->ctx) {
                impl->ctx = ::SSL_CTX_new(::TLS_method());
                if (!impl->ctx) {
                    return SSLAsyncError::set_up_error;
                }
            }
            ::SSL_CTX_set_options(impl->ctx, SSL_OP_NO_SSLv2);
            if (cert) {
                if (!::SSL_CTX_load_verify_locations(impl->ctx, cert, nullptr)) {
                    return SSLAsyncError::cert_register_error;
                }
            }
            if (selfcert) {
                if (!selfprivate) {
                    selfprivate = selfcert;
                }
                if (::SSL_CTX_use_certificate_file(impl->ctx, selfcert, SSL_FILETYPE_PEM) != 1) {
                    return SSLAsyncError::cert_register_error;
                }
                if (::SSL_CTX_use_PrivateKey_file(impl->ctx, selfprivate, SSL_FILETYPE_PEM) != 1) {
                    return SSLAsyncError::cert_register_error;
                }
                if (!::SSL_CTX_check_private_key(impl->ctx)) {
                    return SSLAsyncError::cert_register_error;
                }
            }
            return SSLAsyncError::none;
        }

        bool common_setup_ssl(internal::SSLImpl* impl, const char* alpn, const char* host) {
            if (alpn) {
                if (::SSL_set_alpn_protos(impl->ssl, (const unsigned char*)alpn, ::strlen(alpn)) != 0) {
                    return false;
                }
            }
            if (host) {
                ::SSL_set_tlsext_host_name(impl->ssl, host);
                auto param = SSL_get0_param(impl->ssl);
                if (!X509_VERIFY_PARAM_add1_host(param, host, ::strlen(host))) {
                    return false;
                }
            }
            return true;
        }

        bool common_setup_sync(internal::SSLSyncImpl* impl, IO&& io, const char* cert, const char* alpn, const char* host,
                               const char* selfcert, const char* selfprivate) {
            if (auto err = common_setup_sslctx(impl, cert, selfcert, selfprivate);
                err != SSLAsyncError::none) {
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

        State connecting(internal::SSLSyncImpl* impl) {
        BEGIN:
            if (impl->iostate == State::complete) {
                int res = 0;
                if (impl->is_server) {
                    res = ::SSL_accept(impl->ssl);
                }
                else {
                    res = ::SSL_connect(impl->ssl);
                }
                if (res > 0) {
                    impl->connected = true;
                    impl->iostate = State::complete;
                    return State::complete;
                }
                else if (res == 0) {
                    impl->iostate = State::failed;
                    return State::failed;
                }
                else {
                    if (need_io(impl->ssl)) {
                        impl->iostate = State::running;
                    }
                    else {
                        impl->iostate = State::failed;
                        return State::failed;
                    }
                }
            }
            if (impl->iostate == State::running) {
                impl->iostate = impl->do_IO();
                if (impl->iostate == State::complete) {
                    goto BEGIN;
                }
            }
            return impl->iostate;
        }

#endif
    }  // namespace net
}  // namespace utils
