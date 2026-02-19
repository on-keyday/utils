/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "error_enum.h"
#include <strutil/append.h>

namespace futils::fnet::http3 {
    struct Error {
        H3Error code = H3Error::H3_NO_ERROR;
        bool is_resumable = false;
        const char* debug = nullptr;

        constexpr explicit operator bool() const {
            return code != H3Error::H3_NO_ERROR;
        }

        void error(auto&& pb) {
            strutil::append(pb, "HTTP3Error: code=");
            strutil::append(pb, to_string(code));
            if (debug) {
                strutil::append(pb, ", debug=");
                strutil::append(pb, debug);
            }
            if (is_resumable) {
                strutil::append(pb, ", resumable");
            }
        }
    };

    constexpr Error make_error(H3Error code, const char* debug = nullptr, bool is_resumable = false) {
        return Error{code, is_resumable, debug};
    }
}  // namespace futils::fnet::http3
