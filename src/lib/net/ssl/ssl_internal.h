/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#pragma once
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
                IO io;
                wrap::string buffer;
                SSLIOPhase iophase = SSLIOPhase::read_from_ssl;
                State iostate = State::complete;
                IOMode io_mode = IOMode::idle;
                size_t io_progress = 0;
                bool connected = false;
                State do_IO();

                void clear();

                ~SSLImpl();
#endif
            };
        }  // namespace internal
        bool need_io(::SSL*);
        bool common_setup(internal::SSLImpl* impl, IO&& io, const char* cert, const char* alpn, const char* host,
                          const char* selfcert, const char* selfprivate);
    }  // namespace net
}  // namespace utils
