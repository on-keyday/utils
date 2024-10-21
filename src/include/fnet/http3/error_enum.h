/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

namespace futils {
    namespace fnet::http3 {
        enum class H3Error {
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

        constexpr const char* to_string(H3Error err) {
            switch (err) {
                case H3Error::H3_NO_ERROR:
                    return "H3_NO_ERROR";
                case H3Error::H3_GENERAL_PROTOCOL_ERROR:
                    return "H3_GENERAL_PROTOCOL_ERROR";
                case H3Error::H3_INTERNAL_ERROR:
                    return "H3_INTERNAL_ERROR";
                case H3Error::H3_STREAM_CREATION_ERROR:
                    return "H3_STREAM_CREATION_ERROR";
                case H3Error::H3_CLOSED_CRITICAL_STREAM:
                    return "H3_CLOSED_CRITICAL_STREAM";
                case H3Error::H3_FRAME_UNEXPECTED:
                    return "H3_FRAME_UNEXPECTED";
                case H3Error::H3_FRAME_ERROR:
                    return "H3_FRAME_ERROR";
                case H3Error::H3_EXCESSIVE_LOAD:
                    return "H3_EXCESSIVE_LOAD";
                case H3Error::H3_ID_ERROR:
                    return "H3_ID_ERROR";
                case H3Error::H3_SETTINGS_ERROR:
                    return "H3_SETTINGS_ERROR";
                case H3Error::H3_MISSING_SETTINGS:
                    return "H3_MISSING_SETTINGS";
                case H3Error::H3_REQUEST_REJECTED:
                    return "H3_REQUEST_REJECTED";
                case H3Error::H3_REQUEST_CANCELLED:
                    return "H3_REQUEST_CANCELLED";
                case H3Error::H3_REQUEST_INCOMPLETE:
                    return "H3_REQUEST_INCOMPLETE";
                case H3Error::H3_MESSAGE_ERROR:
                    return "H3_MESSAGE_ERROR";
                case H3Error::H3_CONNECT_ERROR:
                    return "H3_CONNECT_ERROR";
                case H3Error::H3_VERSION_FALLBACK:
                    return "H3_VERSION_FALLBACK";
                case H3Error::QPACK_DECOMPRESSION_FAILED:
                    return "QPACK_DECOMPRESSION_FAILED";
                case H3Error::QPACK_ENCODER_STREAM_ERROR:
                    return "QPACK_ENCODER_STREAM_ERROR";
                case H3Error::QPACK_DECODER_STREAM_ERROR:
                    return "QPACK_DECODER_STREAM_ERROR";
            }
            return "unknown";
        }
    }  // namespace fnet::http3
}  // namespace futils
