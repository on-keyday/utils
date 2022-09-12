/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// stream_state - stream state
#pragma once
#include "../../wrap/light/enum.h"

namespace utils {
    namespace net {
        namespace http2 {
            enum class Status {
                idle,
                closed,
                open,
                half_closed_remote,
                half_closed_local,
                reserved_remote,
                reserved_local,
                unknown,
            };

            BEGIN_ENUM_STRING_MSG(Status, status_name)
            ENUM_STRING_MSG(Status::idle, "idle")
            ENUM_STRING_MSG(Status::closed, "closed")
            ENUM_STRING_MSG(Status::open, "open")
            ENUM_STRING_MSG(Status::half_closed_local, "half_closed_local")
            ENUM_STRING_MSG(Status::half_closed_remote, "half_closed_remote")
            ENUM_STRING_MSG(Status::reserved_local, "reserved_local")
            ENUM_STRING_MSG(Status::reserved_remote, "reserved_remote")
            END_ENUM_STRING_MSG("unknown")

            enum class StreamError {
                none,
                over_max_frame_size,
                continuation_not_followed,
                not_acceptable_on_current_status,
                internal_data_read,
                push_promise_disabled,
                unsupported_frame,
                hpack_failed,
                unimplemented,
                ping_maybe_failed,
                writing_frame,
                reading_frame,
                require_id_0,
                require_id_not_0,
                stream_blocked,
            };

            BEGIN_ENUM_STRING_MSG(StreamError, error_msg)
            ENUM_STRING_MSG(StreamError::over_max_frame_size, "frame size is over max_frame_size")
            ENUM_STRING_MSG(StreamError::continuation_not_followed, "expect continuation frame but not")
            ENUM_STRING_MSG(StreamError::not_acceptable_on_current_status, "such type frame is not acceptable on current status")
            ENUM_STRING_MSG(StreamError::internal_data_read, "error while internal data read")
            ENUM_STRING_MSG(StreamError::push_promise_disabled, "push promise frame is disabled but it was recvieved/sent")
            ENUM_STRING_MSG(StreamError::unsupported_frame, "unspported type frame handled")
            ENUM_STRING_MSG(StreamError::hpack_failed, "hpack encode/decode failed")
            ENUM_STRING_MSG(StreamError::unimplemented, "that feature is unimplemented")
            ENUM_STRING_MSG(StreamError::ping_maybe_failed, "ping maybe failed")
            ENUM_STRING_MSG(StreamError::writing_frame, "error while writing frame")
            ENUM_STRING_MSG(StreamError::reading_frame, "error while reading frame")
            ENUM_STRING_MSG(StreamError::require_id_0, "require id 0 frame")
            ENUM_STRING_MSG(StreamError::require_id_not_0, "require non-zero id frame")
            ENUM_STRING_MSG(StreamError::stream_blocked, "stream now blocking")
            END_ENUM_STRING_MSG("none")

            enum class SettingKey : std::uint16_t {
                table_size = 1,
                enable_push = 2,
                max_concurrent = 3,
                initial_windows_size = 4,
                max_frame_size = 5,
                header_list_size = 6,
            };

            constexpr auto k(SettingKey v) {
                return (std::uint16_t)v;
            }

        }  // namespace http2
    }      // namespace net
}  // namespace utils