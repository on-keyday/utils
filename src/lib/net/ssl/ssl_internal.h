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
                ::BIO* tmpbio = nullptr;

                wrap::string buffer;
                SSLIOPhase iophase = SSLIOPhase::read_from_ssl;
                State iostate = State::complete;
                IOMode io_mode = IOMode::idle;
                size_t io_progress = 0;
                bool connected = false;
                bool is_server = false;

                void clear();

                ~SSLImpl();
#endif
            };

            struct SSLSyncImpl : SSLImpl {
                IO io;
                State do_IO();
            };
        }  // namespace internal
#ifdef USE_OPENSSL
        bool need_io(::SSL*);
        bool setup_ssl(internal::SSLSyncImpl* impl);
        bool common_setup(internal::SSLSyncImpl* impl, IO&& io, const char* cert, const char* alpn, const char* host,
                          const char* selfcert, const char* selfprivate);
        State connecting(internal::SSLSyncImpl* impl);
#endif
    }  // namespace net
}  // namespace utils
