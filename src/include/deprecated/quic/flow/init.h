/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// init - initialize connections
#pragma once
#include <deprecated/quic/common/dll_h.h>
#include "../doc.h"
#include "../transport_param/tpparam.h"
#include "../core/core.h"
#include "../mem/callback.h"
#include "../conn/conn.h"

namespace utils {
    namespace quic {
        namespace flow {
            enum class Error {
                none,
                memory_exhausted,
                invalid_argument,
                crypto_err = 0x1000,
                frame_err = 0x2000,
                packet_err = 0x4000,
            };

            struct InitialConfig {
                const char* host;
                const char* alpn;
                tsize alpn_len;
                const char* cert;
                const char* self_cert;
                const char* private_key;
                const byte* connID;
                tsize connIDlen;
                const byte* token;
                tsize tokenlen;
                byte pnlen;
                tpparam::DefinedParams* transport_param;
                tpparam::TransportParam* custom_param;
                tsize custom_len;
            };

            using DataCallback = mem::CBN<void, conn::Connection*, const byte*, tsize>;

            Dll(Error) start_conn_client(core::QUIC* q, InitialConfig config, DataCallback cb);

        }  // namespace flow
    }      // namespace quic
}  // namespace utils
