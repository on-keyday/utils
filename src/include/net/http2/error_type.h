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
