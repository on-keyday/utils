/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../../include/net/ssl/ssl.h"

#include "../../../include/net/core/platform.h"

#include "../../../include/wrap/lite/string.h"

namespace utils {
    namespace net {
        namespace internal {
            enum class SSLIOPhase {
                read_from_ssl,
                write_to_conn,
                read_from_conn,
                write_to_ssl,
            };

            struct SSLImpl {
#ifdef USE_OPENSSL
                ::SSL* ssl = nullptr;
                ::SSL_CTX* ctx = nullptr;
                ::BIO* bio = nullptr;
                ::BIO* tmpbio = nullptr;
                IO io;
                wrap::string buffer;
                SSLIOPhase iophase = SSLIOPhase::read_from_ssl;
                State iostate = State::complete;
                size_t io_progress = 0;
                bool connected = false;
                State do_IO() {
                    if (iophase == SSLIOPhase::read_from_ssl) {
                        while (true) {
                            char tmpbuf[1024];
                            size_t red = 0;
                            auto result = ::BIO_read_ex(bio, tmpbuf, 1024, &red);
                            buffer.append(tmpbuf, red);
                            if (!result || red < 1024) {
                                break;
                            }
                        }
                        iophase = SSLIOPhase::write_to_conn;
                    }
                    if (iophase == SSLIOPhase::write_to_conn) {
                        if (buffer.size()) {
                            auto e = io.write(buffer.c_str(), buffer.size());
                            if (e != State::complete) {
                                return e;
                            }
                            buffer.clear();
                        }
                        iophase = SSLIOPhase::read_from_conn;
                    }
                    if (iophase == SSLIOPhase::read_from_conn) {
                        auto err = read(buffer, io);
                        if (err != State::complete && err != State::complete) {
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
                        size_t w = 0;
                        auto err = ::BIO_write_ex(bio, buffer.c_str(), buffer.size(), &w);
                        if (!err) {
                            if (!::BIO_should_retry(bio)) {
                                return State::failed;
                            }
                        }
                        else {
                            buffer.clear();
                        }
                        iophase = SSLIOPhase::read_from_ssl;
                    }
                    return State::complete;
                }
#endif
            };
        }  // namespace internal

#ifndef USE_OPENSSL
        std::shared_ptr<SSLConn> SSLResult::connect() {
            return nullptr;
        }

        SSLResult open(IO&& io) {
            IO _ = std::move(io);
            return {};
        }

#else
        bool need_io(::SSL* ssl) {
            auto errcode = ::SSL_get_error(ssl, -1);
            return errcode == SSL_ERROR_WANT_READ || errcode == SSL_ERROR_WANT_WRITE || errcode == SSL_ERROR_SYSCALL;
        }

        static State connecting(internal::SSLImpl* impl) {
        BEGIN:
            if (impl->iostate == State::complete) {
                auto res = ::SSL_connect(impl->ssl);
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

        State SSLConn::write(const char* ptr, size_t size) {
        BEGIN:
            if (impl->iostate == State::complete) {
                size_t w = 0;
                auto res = ::SSL_write_ex(impl->ssl, ptr, size, &w);
                if (!res) {
                    if (!need_io(impl->ssl)) {
                        impl->iostate = State::failed;
                        return State::failed;
                    }
                }
                else {
                    impl->io_progress += w;
                }
            }
            if (impl->iostate == State::running) {
                impl->iostate = impl->do_IO();
                if (impl->iostate == State::complete) {
                    if (impl->io_progress == size) {
                        impl->io_progress = 0;
                        return State::complete;
                    }
                    goto BEGIN;
                }
            }
            return impl->iostate;
        }

        State SSLConn::read(char* ptr, size_t size, size_t* red) {
        }

        void SSLConn::close() {
        }

        wrap::shared_ptr<SSLConn> SSLResult::connect() {
            if (!impl) {
                return nullptr;
            }
            if (failed()) {
                return nullptr;
            }
            if (connecting(impl) == State::complete) {
                auto conn = wrap::make_shared<SSLConn>();
                conn->impl = impl;
                impl = nullptr;
                return conn;
            }
            return nullptr;
        }

        bool SSLResult::failed() {
            return !impl ||
                   impl->iostate != State::complete &&
                       impl->iostate != State::running;
        }

        bool common_setup(internal::SSLImpl* impl, IO&& io, const char* cert, const char* alpn, const char* host,
                          const char* selfcert, const char* selfprivate) {
            if (!impl->ctx) {
                impl->ctx = ::SSL_CTX_new(::TLS_method());
                if (!impl->ctx) {
                    return false;
                }
            }
            ::SSL_CTX_set_options(impl->ctx, SSL_OP_NO_SSLv2);
            if (cert) {
                if (!::SSL_CTX_load_verify_locations(impl->ctx, cert, nullptr)) {
                    return false;
                }
            }
            if (selfcert) {
                if (!selfprivate) {
                    selfprivate = selfcert;
                }
                if (::SSL_CTX_use_certificate_file(impl->ctx, selfcert, SSL_FILETYPE_PEM) != 1) {
                    return false;
                }
                if (::SSL_CTX_use_PrivateKey_file(impl->ctx, selfprivate, SSL_FILETYPE_PEM) != 1) {
                    return false;
                }
                if (!::SSL_CTX_check_private_key(impl->ctx)) {
                    return false;
                }
            }
            if (!::BIO_new_bio_pair(&impl->bio, 0, &impl->tmpbio, 0)) {
                return false;
            }
            impl->ssl = ::SSL_new(impl->ctx);
            if (!impl->ssl) {
                ::BIO_free_all(impl->bio);
                impl->bio = nullptr;
                impl->tmpbio = nullptr;
                return false;
            }
            ::SSL_set_bio(impl->ssl, impl->tmpbio, impl->tmpbio);
            impl->tmpbio = nullptr;
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
            impl->io = std::move(io);
            return true;
        }

        SSLResult open(IO&& io, const char* cert, const char* alpn, const char* host,
                       const char* selfcert, const char* selfprivate) {
            if (io.is_null() || !cert) {
                return SSLResult();
            }
            SSLResult result;
            result.impl = new internal::SSLImpl();
            if (!common_setup(result.impl, std::move(io), cert, alpn, host, selfcert, selfprivate)) {
                return SSLResult();
            }
            auto state = connecting(result.impl);
            if (state != State::complete && state != State::running) {
                return SSLResult();
            }
            return result;
        }
#endif
    }  // namespace net
}  // namespace utils
