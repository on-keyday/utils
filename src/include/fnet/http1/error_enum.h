/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <wrap/light/enum.h>

namespace futils::fnet::http1 {
    namespace header {
        enum class HeaderError {
            none,
            invalid_header,
            invalid_header_key,
            not_colon,
            invalid_header_value,
            validation_error,
            not_end_of_line,

            invalid_method,
            invalid_path,
            invalid_version,
            invalid_status_code,
            invalid_reason_phrase,
            not_space,

            invalid_state,
            no_data,
        };

        constexpr const char* to_string(HeaderError e) {
            switch (e) {
                case HeaderError::none:
                    return "none";
                case HeaderError::invalid_header:
                    return "invalid_header";
                case HeaderError::invalid_header_key:
                    return "invalid_header_key";
                case HeaderError::not_colon:
                    return "not_colon";
                case HeaderError::invalid_header_value:
                    return "invalid_header_value";
                case HeaderError::validation_error:
                    return "validation_error";
                case HeaderError::not_end_of_line:
                    return "not_end_of_line";
                case HeaderError::invalid_method:
                    return "invalid_method";
                case HeaderError::invalid_path:
                    return "invalid_path";
                case HeaderError::invalid_version:
                    return "invalid_version";
                case HeaderError::invalid_status_code:
                    return "invalid_status_code";
                case HeaderError::invalid_reason_phrase:
                    return "invalid_reason_phrase";
                case HeaderError::not_space:
                    return "not_space";
                case HeaderError::invalid_state:
                    return "invalid_state";
                case HeaderError::no_data:
                    return "no_data";
            }
            return "unknown";
        }

        using HeaderErr = wrap::EnumWrap<HeaderError, HeaderError::none, HeaderError::invalid_header>;

    }  // namespace header

    namespace body {
        enum class BodyReadResult {
            full,         // full of body read
            best_effort,  // best effort read (maybe until disconnect) because no `content-length` or `transfer-encoding: chunked` header
            incomplete,   // incomplete read because of no more data

            invalid_state,    // invalid state
            length_mismatch,  // content-length and actual body length mismatch
            bad_space,        // bad space in chunked data
            bad_line,         // invalid line in chunked data
            invalid_header,   // invalid header (both content-length and chunked)
        };

        constexpr const char* to_string(BodyReadResult r) {
            switch (r) {
                case BodyReadResult::full:
                    return "full";
                case BodyReadResult::best_effort:
                    return "best_effort";
                case BodyReadResult::incomplete:
                    return "incomplete";
                case BodyReadResult::invalid_state:
                    return "invalid_state";
                case BodyReadResult::length_mismatch:
                    return "length_mismatch";
                case BodyReadResult::bad_space:
                    return "bad_space";
                case BodyReadResult::bad_line:
                    return "bad_line";
                case BodyReadResult::invalid_header:
                    return "invalid_header";
            }
            return "unknown";
        }
    }  // namespace body

}  // namespace futils::fnet::http1
