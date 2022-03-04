/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// ssl_internal - ssl internal header
#pragma once
#include "../../../include/platform/windows/dllexport_source.h"
#include "../../../include/net/ssl/ssl.h"

#include "../../../include/net/core/platform.h"

#include "../../../include/wrap/lite/string.h"

#include "../../../include/net/generate/iocloser.h"

namespace utils {
    namespace net {
        namespace internal {
            enum class SSLIOPhase {
                read_from_ssl,
                write_to_conn,
                read_from_conn,
                write_to_ssl,
            };

            enum class IOMode {
                idle,
                read,
                write,
                close,
            };

            struct SSLImpl {
#ifdef USE_OPENSSL
                ::SSL* ssl = nullptr;
                ::SSL_CTX* ctx = nullptr;
                ::BIO* bio = nullptr;

                wrap::string buffer;
                SSLIOPhase iophase = SSLIOPhase::read_from_ssl;
                State iostate = State::complete;
                IOMode io_mode = IOMode::idle;
                size_t io_progress = 0;
                bool connected = false;
                bool is_server = false;
                wrap::weak_ptr<SSLConn> conn;

                size_t read_from_ssl(wrap::string& buffer);
                size_t write_to_ssl(wrap::string& buffer);

                void clear();

                ~SSLImpl();
#endif
            };

            struct SSLSyncImpl : SSLImpl {
                IO io;
                State do_IO();
            };

            struct SSLAsyncImpl : SSLImpl {
                AsyncIOClose io;
                bool do_IO(async::Context&);
            };
        }  // namespace internal
#ifdef USE_OPENSSL
        constexpr size_t szfailed = ~0;
        bool need_io(::SSL*);
        bool setup_ssl(internal::SSLImpl* impl);
        bool common_setup_sslctx(internal::SSLImpl* impl, const char* cert, const char* selfcert, const char* selfprivate);
        bool common_setup_ssl(internal::SSLImpl* impl, const char* alpn, const char* host);
        bool common_setup_sync(internal::SSLSyncImpl* impl, IO&& io, const char* cert, const char* alpn, const char* host,
                               const char* selfcert, const char* selfprivate);
        bool common_setup_async(internal::SSLAsyncImpl* impl, AsyncIOClose&& io, const char* cert, const char* alpn, const char* host,
                                const char* selfcert, const char* selfprivate);
        State connecting(internal::SSLSyncImpl* impl);
        State close_impl(internal::SSLSyncImpl* impl);
#endif
    }  // namespace net
}  // namespace utils
