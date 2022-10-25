/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// error - QUIC error code
#pragma once

namespace utils {
    namespace dnet {
        namespace quic {
            // rfc 9000 20.1 Transport Error Codes
            enum class TransportError {
                NO_ERROR = 0x00,
                INTERNAL_ERROR = 0x01,
                CONNECTION_REFUSED = 0x02,
                FLOW_CONTROL_ERROR = 0x03,
                STREAM_LIMIT_ERROR = 0x04,
                STREAM_STATE_ERROR = 0x05,
                FINAL_SIZE_ERROR = 0x06,
                FRAME_ENCODING_ERROR = 0x07,
                TRANSPORT_PARAMETER_ERROR = 0x08,
                CONNECTION_ID_LIMIT_ERROR = 0x09,
                PROTOCOL_VIOLATION = 0x0a,
                INVALID_TOKEN = 0x0b,
                APPLICATION_ERROR = 0x0c,
                CRYPTO_BUFFER_EXCEEDED = 0x0d,
                KEY_UPDATE_ERROR = 0x0e,
                AEAD_LIMIT_REACHED = 0x0f,
                NO_VIABLE_PATH = 0x10,
                CRYPTO_ERROR = 0x100,
            };
        }  // namespace quic
    }      // namespace dnet
}  // namespace utils
