/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// h2err - http2 error
#pragma once

namespace utils {
    namespace dnet {
        enum HTTP2Error {
            http2_error_none,
            http2_invalid_opt,
            http2_resource,
            http2_create_stream,
            http2_invalid_argument,
            http2_need_more_data,
            http2_need_more_buffer,
            http2_invalid_data_length,
            http2_invalid_padding_length,
            http2_invalid_id,
            http2_unknown_type,
            http2_length_consistency,
            http2_invalid_null_data,
            http2_stream_state,
            http2_invalid_data,
            http2_invalid_header_blocks,
            http2_should_reduce_data,
            http2_settings,
            http2_invalid_preface,
            http2_library_bug,
            http2_hpack_error,
            http2_user_error,
        };
    }
}  // namespace utils
