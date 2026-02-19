/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "read_context.h"
#include <view/iovec.h>

namespace futils::fnet::http1 {
    struct RequestLine {
        Range method;
        Range path;
        Range version;

        constexpr view::rvec get_method(view::rvec full_request) {
            return view::rvec(full_request.begin() + method.start, full_request.begin() + method.end);
        }

        constexpr view::rvec get_path(view::rvec full_request) {
            return view::rvec(full_request.begin() + path.start, full_request.begin() + path.end);
        }

        constexpr view::rvec get_version(view::rvec full_request) {
            return view::rvec(full_request.begin() + version.start, full_request.begin() + version.end);
        }
    };

    struct StatusLine {
        Range version;
        Range status_code;
        Range reason_phrase;

        constexpr view::rvec get_version(view::rvec full_response) {
            return view::rvec(full_response.begin() + version.start, full_response.begin() + version.end);
        }

        constexpr view::rvec get_status_code(view::rvec full_response) {
            return view::rvec(full_response.begin() + status_code.start, full_response.begin() + status_code.end);
        }

        constexpr view::rvec get_reason_phrase(view::rvec full_response) {
            return view::rvec(full_response.begin() + reason_phrase.start, full_response.begin() + reason_phrase.end);
        }
    };
}  // namespace futils::fnet::http1
