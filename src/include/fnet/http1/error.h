/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "error_enum.h"
#include <number/to_string.h>

namespace futils::fnet::http1 {
    struct HTTPReadError {
        ReadState state = ReadState::uninit;
        size_t pos = 0;
        header::HeaderError header_error = header::HeaderError::none;
        body::BodyResult body_error = body::BodyResult::full;
        bool is_resumable = false;

        constexpr void error(auto&& pb) {
            strutil::append(pb, "HTTPReadError: state=");
            strutil::append(pb, to_string(state));
            strutil::append(pb, ", pos=");
            number::to_string(pb, pos);
            if (header_error != header::HeaderError::none) {
                strutil::append(pb, ", header_error=");
                strutil::append(pb, header::to_string(header_error));
            }
            if (body_error != body::BodyResult::full) {
                strutil::append(pb, ", body_error=");
                strutil::append(pb, body::to_string(body_error));
            }
            if (is_resumable) {
                strutil::append(pb, ", resumable");
            }
        }

        constexpr explicit operator bool() const {
            return header_error != header::HeaderError::none || body_error != body::BodyResult::full;
        }
    };

    struct HTTPWriteError {
        header::HeaderError header_error = header::HeaderError::none;
        body::BodyResult body_error = body::BodyResult::full;

        constexpr explicit operator bool() const {
            return header_error != header::HeaderError::none || body_error != body::BodyResult::full;
        }

        constexpr void error(auto&& pb) {
            strutil::append(pb, "HTTPWriteError: ");
            if (header_error != header::HeaderError::none) {
                strutil::append(pb, " header_error=");
                strutil::append(pb, header::to_string(header_error));
            }
            if (body_error != body::BodyResult::full) {
                strutil::append(pb, " body_error=");
                strutil::append(pb, body::to_string(body_error));
            }
        }
    };
}  // namespace futils::fnet::http1
