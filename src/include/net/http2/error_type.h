/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// error_type - http2 error definiton
#pragma once
#include "../../wrap/lite/enum.h"

namespace utils {
    namespace net {
        namespace http2 {
            enum class H2Error {
                none = 0x0,
                protocol = 0x1,
                internal = 0x2,
                flow_control = 0x3,
                settings_timeout = 0x4,
                stream_closed = 0x5,
                frame_size = 0x6,
                refused = 0x7,
                cancel = 0x8,
                compression = 0x9,

                connect = 0xa,
                enhance_your_clam = 0xb,
                inadequate_security = 0xc,
                http_1_1_required = 0xd,

                user_defined_bit = 0x100,
                unknown,
                read_len,
                read_type,
                read_flag,
                read_id,
                read_padding,
                read_data,
                read_depend,
                read_weight,
                read_code,
                read_increment,
                read_processed_id,

                type_mismatch,
                transport,
                ping_failed,
            };

            DEFINE_ENUM_FLAGOP(H2Error)

            BEGIN_ENUM_STRING_MSG(H2Error, error_msg)
            ENUM_STRING_MSG(H2Error::none, "no error")
            ENUM_STRING_MSG(H2Error::protocol, "protocol error")
            ENUM_STRING_MSG(H2Error::internal, "internal error")
            ENUM_STRING_MSG(H2Error::flow_control, "flow control error")
            ENUM_STRING_MSG(H2Error::settings_timeout, "settings timeout")
            ENUM_STRING_MSG(H2Error::stream_closed, "stream closed")
            ENUM_STRING_MSG(H2Error::frame_size, "frame size error")
            ENUM_STRING_MSG(H2Error::refused, "stream refused")
            ENUM_STRING_MSG(H2Error::cancel, "canceled")
            ENUM_STRING_MSG(H2Error::compression, "compression error")
            ENUM_STRING_MSG(H2Error::connect, "connect error")
            ENUM_STRING_MSG(H2Error::enhance_your_clam, "enchance your clam")
            ENUM_STRING_MSG(H2Error::inadequate_security, "inadequate security")
            ENUM_STRING_MSG(H2Error::http_1_1_required, "http/1.1 required")
            ENUM_STRING_MSG(H2Error::transport,"transport layer error")
            END_ENUM_STRING_MSG("unknown or internal error")

            enum class FrameType : std::uint8_t {
                data = 0x0,
                header = 0x1,
                priority = 0x2,
                rst_stream = 0x3,
                settings = 0x4,
                push_promise = 0x5,
                ping = 0x6,
                goaway = 0x7,
                window_update = 0x8,
                continuous = 0x9,
            };

            BEGIN_ENUM_STRING_MSG(FrameType, frame_name)
            ENUM_STRING_MSG(FrameType::data, "data")
            ENUM_STRING_MSG(FrameType::header, "header")
            ENUM_STRING_MSG(FrameType::priority, "priority")
            ENUM_STRING_MSG(FrameType::rst_stream, "rst_stream")
            ENUM_STRING_MSG(FrameType::settings, "settings")
            ENUM_STRING_MSG(FrameType::push_promise, "push_promise")
            ENUM_STRING_MSG(FrameType::ping, "ping")
            ENUM_STRING_MSG(FrameType::goaway, "goaway")
            ENUM_STRING_MSG(FrameType::window_update, "window_update")
            ENUM_STRING_MSG(FrameType::continuous, "continuous")
            END_ENUM_STRING_MSG("unknown")

            enum Flag : std::uint8_t {
                none = 0x0,
                ack = 0x1,
                end_stream = 0x1,
                end_headers = 0x4,
                padded = 0x8,
                priority = 0x20,
            };

            DEFINE_ENUM_FLAGOP(Flag)

            struct Dummy {};

        }  // namespace http2
    }      // namespace net
}  // namespace utils
