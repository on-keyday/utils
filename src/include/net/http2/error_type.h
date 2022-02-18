/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// error_type - http2 error definiton

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

                unknown = 0xff,
                read_len,
                read_type,
                read_flag,
                read_id,
                read_padding,
                read_data,
                read_depend,
                read_weight,
                read_code,
            };
        }

        struct Dummy {};
    }  // namespace net
}  // namespace utils
