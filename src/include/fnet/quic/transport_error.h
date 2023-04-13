/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// error - QUIC error code
#pragma once
#include "../../core/byte.h"
#include "../../helper/appender.h"
#include "../error.h"
#include "types.h"

namespace utils {
    namespace fnet {
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

            constexpr TransportError to_CRYPTO_ERROR(byte b) {
                return TransportError(int(TransportError::CRYPTO_ERROR) + b);
            }

            constexpr bool is_CRYPTO_ERROR(TransportError err) {
                return int(err) >= 0x100 && int(err) <= 0x1FF;
            }

            constexpr const char* errmsg(TransportError err) {
                switch (err) {
#define CASE(err)             \
    case TransportError::err: \
        return #err;
                    CASE(NO_ERROR)
                    CASE(INTERNAL_ERROR)
                    CASE(CONNECTION_REFUSED)
                    CASE(FLOW_CONTROL_ERROR)
                    CASE(STREAM_LIMIT_ERROR)
                    CASE(STREAM_STATE_ERROR)
                    CASE(FINAL_SIZE_ERROR)
                    CASE(FRAME_ENCODING_ERROR)
                    CASE(TRANSPORT_PARAMETER_ERROR)
                    CASE(CONNECTION_ID_LIMIT_ERROR)
                    CASE(PROTOCOL_VIOLATION)
                    CASE(INVALID_TOKEN)
                    CASE(APPLICATION_ERROR)
                    CASE(CRYPTO_BUFFER_EXCEEDED)
                    CASE(KEY_UPDATE_ERROR)
                    CASE(AEAD_LIMIT_REACHED)
                    CASE(NO_VIABLE_PATH)
                    default: {
                        if (is_CRYPTO_ERROR(err)) {
                            return "CRYPTO_ERROR";
                        }
                        return nullptr;
                    }
                }
#undef CASE
            }

            struct QUICError {
                const char* msg = nullptr;
                TransportError transport_error = TransportError::INTERNAL_ERROR;
                PacketType packet_type = PacketType::Unknown;
                FrameType frame_type = FrameType::PADDING;
                error::Error base;
                bool is_app = false;
                bool by_peer = false;

                void error(auto&& pb) {
                    helper::appends(pb,
                                    "quic", is_app ? "(app)" : "", ": ",
                                    msg);
                    if (by_peer) {
                        helper::append(pb, " by peer");
                    }
                    if (transport_error != TransportError::NO_ERROR) {
                        helper::append(pb, " transport_error=");
                        if (auto msg = errmsg(transport_error)) {
                            helper::append(pb, msg);
                            if (is_CRYPTO_ERROR(transport_error)) {
                                helper::append(pb, "(");
                                number::to_string(pb, int(transport_error) & 0xFF, 16);
                                helper::append(pb, ")");
                            }
                        }
                        else {
                            helper::append(pb, "TransportError(");
                            number::to_string(pb, int(transport_error));
                            helper::append(pb, ")");
                        }
                    }
                    if (frame_type != FrameType::PADDING) {
                        helper::append(pb, " frame_type=");
                        if (auto msg = to_string(frame_type)) {
                            helper::append(pb, msg);
                            if (is_STREAM(frame_type)) {
                                if (FrameFlags{frame_type}.STREAM_off()) {
                                    helper::append(pb, "|OFF");
                                }
                                if (FrameFlags{frame_type}.STREAM_len()) {
                                    helper::append(pb, "|LEN");
                                }
                                if (FrameFlags{frame_type}.STREAM_fin()) {
                                    helper::append(pb, "|FIN");
                                }
                            }
                        }
                        else {
                            helper::append(pb, "FrameType(");
                            number::to_string(pb, FrameFlags{frame_type}.value);
                            helper::append(pb, ")");
                        }
                    }
                    if (packet_type != PacketType::Unknown) {
                        helper::append(pb, "packet_type=");
                        if (auto msg = to_string(packet_type)) {
                            helper::append(pb, msg);
                        }
                        else {
                            helper::append(pb, "PacketType(");
                            number::to_string(pb, int(packet_type));
                            helper::append(pb, ")");
                        }
                    }
                    if (base != error::none) {
                        helper::append(pb, " base=");
                        base.error(pb);
                    }
                }

                error::ErrorCategory category() {
                    return error::ErrorCategory::quicerr;
                }

                error::Error unwrap() {
                    return base;
                }

                std::uint64_t errnum() const {
                    return (std::uint64_t)transport_error;
                }
            };
        }  // namespace quic
    }      // namespace fnet
}  // namespace utils
