/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

namespace futils {
    namespace fnet::http3 {
        enum class Error {
            H3_NO_ERROR = 0x0100,
            H3_GENERAL_PROTOCOL_ERROR = 0x0101,
            H3_INTERNAL_ERROR = 0x0102,
            H3_STREAM_CREATION_ERROR = 0x0103,
            H3_CLOSED_CRITICAL_STREAM = 0x0104,
            H3_FRAME_UNEXPECTED = 0x0105,
            H3_FRAME_ERROR = 0x0106,
            H3_EXCESSIVE_LOAD = 0x0107,
            H3_ID_ERROR = 0x0108,
            H3_SETTINGS_ERROR = 0x0109,
            H3_MISSING_SETTINGS = 0x010a,
            H3_REQUEST_REJECTED = 0x010b,
            H3_REQUEST_CANCELLED = 0x010c,
            H3_REQUEST_INCOMPLETE = 0x010d,
            H3_MESSAGE_ERROR = 0x010e,
            H3_CONNECT_ERROR = 0x010f,
            H3_VERSION_FALLBACK = 0x0110,

            QPACK_DECOMPRESSION_FAILED = 0x0200,
            QPACK_ENCODER_STREAM_ERROR = 0x0201,
            QPACK_DECODER_STREAM_ERROR = 0x0202,
        };
    }
}  // namespace futils
