/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/cnet/ssl.h"
#include "../../include/wrap/light/string.h"
#include "../../include/number/array.h"
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
            };

            bool open_tls(CNet* ctx, OpenSSLContext* tls) {
                tls->sslctx = ::SSL_CTX_new(TLS_method());
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
            }

            void close_ssl(CNet* ctx, OpenSSLContext* tls) {
                ::SSL_free(tls->ssl);
                tls->ssl = nullptr;
                tls->bio = nullptr;
                ::SSL_CTX_free(tls->sslctx);
                tls->sslctx = nullptr;
                return;
            }

            CNet* STDCALL create_client() {
                ProtocolSuite<OpenSSLContext> ssl_proto{
                    .initialize = open_tls,
                    .deleter = [](OpenSSLContext* v) { delete v; },
                };
                return create_cnet(CNetFlag::once_set_no_delete_link | CNetFlag::init_before_io, new OpenSSLContext{}, ssl_proto);
            }
#endif
        }  // namespace ssl
    }      // namespace cnet
}  // namespace utils
