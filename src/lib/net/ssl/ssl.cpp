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
        SSLResult open(IO&& io) {
            IO _ = std::move(io);
            return {};
        }
#else

        SSLResult open(IO&& io, const char* cert, const char* alpn,
                       const char* selfcert, const char* selfprivate) {
            if (io.is_null() || !cert) {
                return SSLResult();
            }
            SSLResult ret;
            ret.impl = new internal::SSLImpl();
            auto impl = ret.impl;
            impl->ctx = ::SSL_CTX_new(TLS_method());
            if (!impl->ctx) {
                return SSLResult();
            }
            ::SSL_CTX_set_options(impl->ctx, SSL_OP_NO_SSLv2);
            if (!::SSL_CTX_load_verify_locations(impl->ctx, cert, nullptr)) {
                return SSLResult();
            }
            if (selfcert && !selfprivate) {
                selfprivate = selfcert;
            }
        }
#endif
    }  // namespace net
}  // namespace utils
